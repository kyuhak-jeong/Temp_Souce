#ifndef SVMTRACKER_H_
#define SVMTRACKER_H_
#include <map>
#include <vector>
#include "svmTrack.h"
#include "svmMunkres.h"

class sanTracker 
{
public:
    sanTracker(int _coast_cycles_threshold = 1);
	~sanTracker();

    std::vector<EXT_OBJECT> Run(const std::vector<ORG_OBJECT>& detections);
    std::map<int, sanTrack>& GetTracks();
    static float CalculateIoU(const ORG_OBJECT& detection1, const ORG_OBJECT& detection2); // Intersection over Union

private:
    static float CalculateIoU(const ORG_OBJECT& detection, const sanTrack& track); // Intersection over Union

    static void HungarianMatching(const std::vector<std::vector<float>>& iou_matrix, size_t nrows, size_t ncols, 
                                  std::vector<std::vector<float>>& association /*[out]*/);

    static void AssociateDetectionsToTrackers(const std::vector<ORG_OBJECT>& detections,     /*[in]detected objects */
                                               std::map<int, sanTrack>& tracks,          /*[in] manangement pool */
                                               std::map<int, ORG_OBJECT>& matched_detections /*[out]*/,
                                               std::vector<ORG_OBJECT>& unmatched_detections /*[out]*/,
                                               float iou_threshold = 0.3f);              /*[in] matching rate 0.0 ~ 0.5*/

    std::map<int, sanTrack> m_tracks;     // Hash-map between ID and corresponding tracker
    int m_id_sequence; // Assigned ID for each bounding box
    int m_coast_cycles_threshold; 

public:

    static std::vector<ORG_OBJECT> refineObjects(const std::vector<ORG_OBJECT>& detections/*[in]*/, float iou_threshold /*[in]*/);
};

#endif
