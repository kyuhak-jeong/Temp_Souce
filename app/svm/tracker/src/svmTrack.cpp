#include "svmTrack.h"


#if (0) 
sanTrack::sanTrack() : kf_(8, 4) 
{
    // state-transition matrix - center_x, center_y, width, height, v_cx, v_cy, v_width, v_height
    kf_.F_ <<
            1, 0, 0, 0, 1, 0, 0, 0,    // x + vx
            0, 1, 0, 0, 0, 1, 0, 0,    // y + vy
            0, 0, 1, 0, 0, 0, 1, 0,    // width + vwidth
            0, 0, 0, 1, 0, 0, 0, 1,    // height + vheight
            0, 0, 0, 0, 1, 0, 0, 0,    // vx
            0, 0, 0, 0, 0, 1, 0, 0,    // vy
            0, 0, 0, 0, 0, 0, 1, 0,    // vwidth
            0, 0, 0, 0, 0, 0, 0, 1;    // vheight

    // error covariance matrix, give high uncertainty to the unobservable initial velocities
    kf_.P_ <<
            10, 0,  0,  0,     0,     0,     0,     0, // x
            0, 10,  0,  0,     0,     0,     0,     0, // y
            0,  0, 10,  0,     0,     0,     0,     0, // width
            0,  0,  0, 10,     0,     0,     0,     0, // height
            0,  0,  0,  0, 10000,     0,     0,     0, // vx
            0,  0,  0,  0,     0, 10000,     0,     0, // vy
            0,  0,  0,  0,     0,     0, 10000,     0, // vwidth
            0,  0,  0,  0,     0,     0,     0, 10000; // vheight


    //observation(measurement) matrix
    kf_.H_ <<
            1, 0, 0, 0, 0, 0, 0, 0, //x
            0, 1, 0, 0, 0, 0, 0, 0, //y
            0, 0, 1, 0, 0, 0, 0, 0, //width
            0, 0, 0, 1, 0, 0, 0, 0; //height

    // covariance matrix of process noise
    kf_.Q_ <<
            1, 0, 0, 0,    0,    0,      0,      0,  // x
            0, 1, 0, 0,    0,    0,      0,      0,  // y
            0, 0, 1, 0,    0,    0,      0,      0,  // width
            0, 0, 0, 1,    0,    0,      0,      0,  // height
            0, 0, 0, 0, 0.01,    0,      0,      0,  // vx
            0, 0, 0, 0,    0, 0.01,      0,      0,  // vy
            0, 0, 0, 0,    0,    0, 0.0001,      0,  // vwidth
            0, 0, 0, 0,    0,    0,      0, 0.0001;  // vheight

    // covariance matrix of observation noise
    kf_.R_ <<
            1, 0,  0,   0, // x
            0, 1,  0,   0, // y
            0, 0, 10,   0, // width
            0, 0,  0,  10; // height
}

#else

sanTrack::sanTrack() : kf_(8, 4)
{
    // state-transition matrix - center_x, center_y, width, height, v_cx, v_cy, v_width, v_height
    kf_.F_ <<
        1, 0, 0, 0, 1, 0, 0, 0,    // center_x
        0, 1, 0, 0, 0, 1, 0, 0,    // center_y
        0, 0, 1, 0, 0, 0, 1, 0,    // width
        0, 0, 0, 1, 0, 0, 0, 1,    // height
        0, 0, 0, 0, 1, 0, 0, 0,    // center_vx
        0, 0, 0, 0, 0, 1, 0, 0,    // center_vy
        0, 0, 0, 0, 0, 0, 1, 0,    // vwidth
        0, 0, 0, 0, 0, 0, 0, 1;    // vheight

   // error covariance matrix, give high uncertainty to the unobservable initial velocities
    kf_.P_ <<
        10, 0, 0, 0, 0, 0, 0, 0,      // center_x
        0, 10, 0, 0, 0, 0, 0, 0,      // center_y
        0, 0, 20, 0, 0, 0, 0, 0,      // width
        0, 0, 0, 20, 0, 0, 0, 0,      // height
        0, 0, 0, 0, 1000000, 0, 0, 0,   // center_vx
        0, 0, 0, 0, 0, 1000000, 0, 0,   // center_vy
        0, 0, 0, 0, 0, 0, 1000000, 0,  // vwidth
        0, 0, 0, 0, 0, 0, 0, 1000000;  // vheight

    //observation(measurement) matrix
    kf_.H_ <<
        1, 0, 0, 0, 0, 0, 0, 0, //center_x
        0, 1, 0, 0, 0, 0, 0, 0, //center_y
        0, 0, 1, 0, 0, 0, 0, 0, //width
        0, 0, 0, 1, 0, 0, 0, 0; //height

    // covariance matrix of process noise
    kf_.Q_ <<
        1, 0, 0, 0, 0, 0, 0, 0,  // center_x
        0, 1, 0, 0, 0, 0, 0, 0,  // center_y
        0, 0, 1, 0, 0, 0, 0, 0,  // width
        0, 0, 0, 1, 0, 0, 0, 0,  // height
        0, 0, 0, 0, 0.000001, 0, 0, 0,  // center_vx
        0, 0, 0, 0, 0, 0.000001, 0, 0,  // center_vy
        0, 0, 0, 0, 0, 0, 0.000001, 0,  // vwidth
        0, 0, 0, 0, 0, 0, 0, 0.000001;  // vheight

    // covariance matrix of observation(measurement) noise
    kf_.R_ <<
        100, 0, 0, 0, // center_x
        0, 100, 0, 0, // center_y
        0, 0, 100, 0, // width
        0, 0, 0, 100; // height
}

#endif


// Get predicted locations from existing trackers
// dt is time elapsed between the current and previous measurements
void sanTrack::Predict() 
{
    kf_.Predict();

    // hit streak count will be reset
    if (m_coast_cycles > 0)
        m_hit_streak = 0;

    // accumulate coast cycle count
    m_coast_cycles++;
}


// Update matched trackers with assigned detections
void sanTrack::Update(const ORG_OBJECT& bbox) 
{
    m_cam_id = bbox.cam_id;
    m_class_id = bbox.class_id;
    m_timestamp = bbox.timestamp;
    m_coast_cycles = 0; // get measurement update, reset coast cycle count
    m_hit_streak++; // accumulate hit streak count

    // observation - center_x, center_y, area, ratio
    Eigen::VectorXd observation = ConvertBboxToObservation(bbox);
    kf_.Update(observation);
}


// Create and initialize new trackers for unmatched detections, with initial bounding box
void sanTrack::Init(const ORG_OBJECT &bbox) 
{
    m_cam_id = bbox.cam_id;
    m_class_id = bbox.class_id;
    m_timestamp = bbox.timestamp;
    kf_.x_.head(4) << ConvertBboxToObservation(bbox);
    m_coast_cycles = 0; // get measurement update, reset coast cycle count
    m_hit_streak++;
}


ORG_OBJECT sanTrack::GetStateAsObject() const 
{
    return ConvertStateToBbox(kf_.x_);
}


float sanTrack::GetNIS() const 
{
    return kf_.NIS_;
}


/**
 * Takes a bounding box in the form [x, y, width, height] and returns z in the form
 * [x, y, s, r] where x,y is the centre of the box and s is the scale/area and r is
 * the aspect ratio
 *
 * @param bbox
 * @return
 */
Eigen::VectorXd sanTrack::ConvertBboxToObservation(const ORG_OBJECT& bbox) const
{
    Eigen::VectorXd observation = Eigen::VectorXd::Zero(4);

    auto width = static_cast<float>(bbox.width);
    auto height = static_cast<float>(bbox.height);

    float center_x = bbox.x + width / 2;
    float center_y = bbox.y + height / 2;

    observation << center_x, center_y, width, height;

    return observation;
}


/**
 * Takes a bounding box in the centre form [x,y,s,r] and returns it in the form
 * [x1,y1,x2,y2] where x1,y1 is the top left and x2,y2 is the bottom right
 *
 * @param state
 * @return
 */
ORG_OBJECT sanTrack::ConvertStateToBbox(const Eigen::VectorXd &state) const 
{
    // state - center_x, center_y, width, height, v_cx, v_cy, v_width, v_height
    auto width = std::max(0, static_cast<int>(state[2]));
    auto height = std::max(0, static_cast<int>(state[3]));

    auto x = static_cast<int>(state[0] - width / 2.0);
    auto y = static_cast<int>(state[1] - height / 2.0);

    return ORG_OBJECT(0, 0, x, y, width, height, 0);
}
