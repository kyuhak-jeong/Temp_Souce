#include <exception>
#include "svmTracker.h"
#include "svmCore.hpp"
#include "svmLogger.hpp"

sanTracker::sanTracker(int _coast_cycles_threshold) : m_coast_cycles_threshold(_coast_cycles_threshold)
{
    m_id_sequence = 0;
}

sanTracker::~sanTracker()
{
    if (!m_tracks.empty()) m_tracks.clear(); else noop;
}

float sanTracker::CalculateIoU(const ORG_OBJECT& detection, const sanTrack& track) // Intersion over Union
{
    auto trk = track.GetStateAsObject();
    // get min/max points
    auto xx1 = std::max(detection.x, trk.x);
    auto yy1 = std::max(detection.y, trk.y);
    auto xx2 = std::min(detection.x + detection.width, trk.x + trk.width);
    auto yy2 = std::min(detection.y + detection.height, trk.y + trk.height);
    auto w = std::max(0, xx2 - xx1);
    auto h = std::max(0, yy2 - yy1);

    // calculate area of intersection and union
    float det_area = (float)(detection.width * detection.height);
    float trk_area = (float)(trk.width * trk.height);
    auto intersection_area = w * h;
    float union_area = det_area + trk_area - intersection_area;
    auto iou = intersection_area / union_area;

    return iou;
}


float sanTracker::CalculateIoU(const ORG_OBJECT& detection1, const ORG_OBJECT& detection2) // Intersion over Union
{
    // auto trk = track.GetStateAsObject();
    // get min/max points
    auto xx1 = std::max(detection1.x, detection2.x);
    auto yy1 = std::max(detection1.y, detection2.y);
    auto xx2 = std::min(detection1.x + detection1.width, detection2.x + detection2.width);
    auto yy2 = std::min(detection1.y + detection1.height, detection2.y + detection2.height);
    auto w = std::max(0, xx2 - xx1);
    auto h = std::max(0, yy2 - yy1);

    // calculate area of intersection and union
    float detection1_area = (float)(detection1.width * detection1.height);
    float detection2_area = (float)(detection2.width * detection2.height);
    auto intersection_area = w * h;
    float union_area = detection1_area + detection2_area - intersection_area;
    auto iou = intersection_area / union_area;

    return iou;
}


void sanTracker::HungarianMatching(const std::vector<std::vector<float>>& iou_matrix, size_t nrows, size_t ncols, std::vector<std::vector<float>>& association /*[out]*/)
{
    Matrix<float> matrix(nrows, ncols);

    // Initialize matrix with IoU values
    for (size_t i = 0 ; i < nrows ; i++) 
    {
        for (size_t j = 0 ; j < ncols ; j++) 
        {
            // Multiply by -1 to find max cost
            if (iou_matrix[i][j] != 0) 
            {
                matrix(i, j) = -iou_matrix[i][j];
            }
            else 
            {
                // TODO: figure out why we have to assign value to get correct result
                matrix(i, j) = 1.0f;
            }
        }
    }
     
#if (0) // Display begin matrix state for debugging
    for (size_t row = 0 ; row < nrows ; row++) 
    {
        for (size_t col = 0 ; col < ncols ; col++) 
        {
            std::cout.width(10);
            std::cout << matrix(row,col) << ",";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
#endif

    // Apply Kuhn-Munkres algorithm to matrix.
    sanMunkres<float> munkres;
    munkres.solve(matrix);
    
#if  (0) // Display solved matrix for debugging
    for (size_t row = 0 ; row < nrows ; row++) 
    {
        for (size_t col = 0 ; col < ncols ; col++) 
        {
            std::cout.width(2);
            std::cout << matrix(row,col) << ",";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
#endif

    for (size_t i = 0 ; i < nrows ; i++) 
    {
        for (size_t j = 0 ; j < ncols ; j++) 
        {
            association[i][j] = matrix(i, j); 
        }
    }
}

std::vector<ORG_OBJECT> sanTracker::refineObjects(const std::vector<ORG_OBJECT>& _detections/*[in]*/, float iou_threshold /*[in]*/)
{
    std::vector<ORG_OBJECT> unique_detections;

    if (_detections.size())
    {
        // remove object with minimum size;
        std::vector<ORG_OBJECT> detections;
        for (auto& iter:_detections)
        {
            if((3 < iter.width)&&(5 < iter.height))
                detections.push_back(iter);
        }

        std::vector<std::vector<float>> iou_matrix;
        // resize IoU matrix based on number of detection and tracks
        iou_matrix.resize(detections.size(), std::vector<float>(detections.size()));

        for (size_t i = 0; i < detections.size(); i++)
        {
            for (size_t j = 0; j < detections.size(); j++)
            {
                iou_matrix[i][j] = CalculateIoU(detections[i], detections[j]);
            }
        }

        std::map<int, ORG_OBJECT> duplicate_detection_map;
        for (size_t i = 0; i < detections.size(); i++)
        {
            for (size_t j = i + 1; j < detections.size(); j++)
            {
                if (iou_threshold <= iou_matrix[i][j])
                {
                    duplicate_detection_map[(int)j] = detections[j];
                }
                else noop;
            }
        }

        for (int idx = 0; idx < (int)detections.size(); idx++)
        {
            auto dupl_iter = duplicate_detection_map.find(idx);
            if (dupl_iter == duplicate_detection_map.end())
            {
                unique_detections.push_back(detections[idx]);
            }
            else noop;
        }
    }
    else noop;
    
    return unique_detections;
}



void sanTracker::AssociateDetectionsToTrackers(const std::vector<ORG_OBJECT>& detections/*[in]*/,
                                            std::map<int, sanTrack>& tracks/*[in]*/,
                                            std::map<int, ORG_OBJECT>& matched_detections /*[out]*/,
                                            std::vector<ORG_OBJECT>& unmatched_detections/*[out]*/,
                                            float iou_threshold /*[in]*/)
{
    // Set all detection as unmatched if no tracks existing
    if (tracks.empty()) 
    {
        for (const auto& detection : detections) 
            unmatched_detections.push_back(detection);
    }
    else
    {
        std::vector<std::vector<float>> iou_matrix;
        // resize IoU matrix based on number of detection and tracks
        iou_matrix.resize(detections.size(), std::vector<float>(tracks.size()));

        std::vector<std::vector<float>> association_matrix;
        // resize association matrix based on number of detection and tracks
        association_matrix.resize(detections.size(), std::vector<float>(tracks.size()));

        // row - detection, column - tracks
        for (size_t i = 0; i < detections.size(); i++)
        {
            size_t j = 0;
            for (const auto& track : tracks)
            {
                iou_matrix[i][j] = CalculateIoU(detections[i], track.second);
                j++;
            }
        }

        // Find association
        HungarianMatching(iou_matrix, detections.size(), tracks.size(), association_matrix);

        for (size_t i = 0; i < detections.size(); i++)  //  row index
        {
            bool matched_flag = false;
            size_t j = 0; // column index
            for (const auto& track : tracks)
            {
                if (fabs(association_matrix[i][j]) <= FLT_EPSILON)
                {
                    // Filter out matched with low IoU
                    if (iou_matrix[i][j] >= iou_threshold) // matched
                    {
                        matched_detections[track.first] = detections[i];
                        matched_flag = true;
                    }
                    else noop;

                    // It builds 1 to 1 association, so we can break from here
                    break;
                }
                j++;
            }
            if (!matched_flag)
                unmatched_detections.push_back(detections[i]);
            else noop;

        }
    }
}


std::vector<EXT_OBJECT> sanTracker::Run(const std::vector<ORG_OBJECT>& detections) 
{
    std::vector<EXT_OBJECT> frame_extended_objects;

    try
    {
        std::map<int, sanTrack> frame_detections;

        /*** Predict internal tracks from previous frame ***/
        for (auto& track : m_tracks)
            track.second.Predict();

        std::map<int, ORG_OBJECT> matched_detections; // Hash-map between track ID and associated detection bounding box 
        std::vector<ORG_OBJECT> unmatched_detections; // vector of unassociated detections
        if (!detections.empty())
            AssociateDetectionsToTrackers(detections, m_tracks, matched_detections, unmatched_detections);

#if (0)
        std::cout << "detections: " << detections.size() << std::endl;
        std::cout << "m_tracks: " << m_tracks.size() << std::endl;
        std::cout << "matched: " << matched_detections.size() << std::endl;
        std::cout << "unmatched: " << unmatched_detections.size() << std::endl;
#endif

        /*** Update tracks with associated bbox ***/
        for (const auto& matched : matched_detections)
        {
            m_tracks[matched.first].Update(matched.second);
            frame_detections[matched.first] = m_tracks[matched.first];
        }

        /*** Create new tracks for unmatched detections ***/
        for (const auto& unmatched : unmatched_detections)
        {
            sanTrack tracker;
            tracker.Init(unmatched);

            // Create new track and generate new ID
            m_tracks[m_id_sequence] = tracker;
            m_tracks[m_id_sequence].m_object_id = m_id_sequence;
            frame_detections[m_id_sequence] = m_tracks[m_id_sequence];

            m_id_sequence++;
            m_id_sequence = m_id_sequence % (INT_MAX - 1);
        }

        /*** Delete lose tracked tracks ***/
        for (auto it = m_tracks.begin(); it != m_tracks.end();)
        {
            if (m_coast_cycles_threshold < it->second.m_coast_cycles)
                it = m_tracks.erase(it);
            else
                it++;
        }

        /*** Update track information  ***/
        for (auto& track : m_tracks)
        {
            ORG_OBJECT object = track.second.GetStateAsObject();
            track.second.m_x = object.x;
            track.second.m_y = object.y;
            track.second.m_width = object.width;
            track.second.m_height = object.height;
        }
       
        /*** convert datatype ***/
        for (auto& frame_detection : frame_detections)
        {
            EXT_OBJECT extended_object;
            int id = frame_detection.second.m_object_id;
            extended_object.cam_id = m_tracks[id].m_cam_id;
            extended_object.class_id = m_tracks[id].m_class_id;
            extended_object.object_id = m_tracks[id].m_object_id;
            extended_object.x = m_tracks[id].m_x;
            extended_object.y = m_tracks[id].m_y;
            extended_object.width = m_tracks[id].m_width;
            extended_object.height = m_tracks[id].m_height;
            extended_object.timestamp = m_tracks[id].m_timestamp;
            extended_object.coast_cycles = m_tracks[id].m_coast_cycles;
            extended_object.hit_streak = m_tracks[id].m_hit_streak;

            frame_extended_objects.push_back(extended_object);
        }
    }
    catch (std::exception& e)
    {
        throw logger.svm_fatal("C2207101", __FUNCTION__ + delimiter(string(e.what())) + string(". failed to track objects"));
    }

    return frame_extended_objects;
}


std::map<int, sanTrack>& sanTracker::GetTracks()
{
    return m_tracks;
}
