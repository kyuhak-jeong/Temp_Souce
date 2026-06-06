#ifndef SVMTRACK_H_
#define SVMTRACK_H_

#include "svmKF.h"

struct ORG_OBJECT  // detector -> tracker
{
    int cam_id;
    int class_id;
    int x;
    int y;
    int width;
    int height;
    clock_t timestamp;

    ORG_OBJECT(int _cam_id = 0, int _class_id = 0, int _x = 0, int _y = 0, int _width = 0, int _height = 0, clock_t _timestamp=0) : cam_id(_cam_id), class_id(_class_id), x(_x), y(_y), width(_width), height(_height), timestamp(_timestamp) {}
};

struct EXT_OBJECT // tracker -> svm
{
    int cam_id;
    int class_id;
    int object_id;
    int x;
    int y;
    int width;
    int height;
    clock_t timestamp;
    int coast_cycles;
    int hit_streak;

    EXT_OBJECT(int _cam_id = 0, int _class_id = 0, int _object_id = 0, int _x = 0, int _y = 0, int _width = 0, int _height = 0, clock_t _timestamp = 0, int _coast_cycles=0, int _hit_streak=0) : 
        cam_id(_cam_id), class_id(_class_id), object_id(_object_id), x(_x), y(_y), width(_width), height(_height), timestamp(_timestamp), coast_cycles(_coast_cycles), hit_streak(_hit_streak) {}
};



class sanTrack 
{
public:
    int m_cam_id = 0;
    int m_class_id = 0; // a given value, when Init() or Update() are called
    int m_object_id = 0;
    int m_x = 0;  // based on the quadrant image
    int m_y = 0;  // based on the quadrant image
    int m_width = 0; // based on the quadrant image
    int m_height = 0; // based on the quadrant image
    clock_t m_timestamp = 0; // a given value, when Init() or Update() are called

    int m_coast_cycles = 0; // undetected counter after detecting
    int m_hit_streak = 0;  // continual detected counter
    sanKF kf_; /*kalman filter: x, y, width, height*/

public:
    sanTrack();
    ~sanTrack() = default;

    void Init(const ORG_OBJECT& bbox);
    void Predict();
    void Update(const ORG_OBJECT& bbox);
    ORG_OBJECT GetStateAsObject() const; // current kf_.x_
    float GetNIS() const;

    Eigen::VectorXd ConvertBboxToObservation(const ORG_OBJECT& bbox) const;
    ORG_OBJECT ConvertStateToBbox(const Eigen::VectorXd& state) const;

};

#endif
