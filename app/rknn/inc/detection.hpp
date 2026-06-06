#ifndef DETECTION_APP_HPP_
#define DETECTION_APP_HPP_

// syan for C++ headers
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <future>
#include "object_detection.hpp"
#include "time_rate.hpp"

using ImagePtr = uint8_t*;
using Objects = std::vector<Object>;

typedef struct {
    std::string rknn_path;
    uint8_t class_num;
    float box_thres;
    float nms_thres;
} detection_app_info;

typedef struct {
    int width;
    int height;
} input_size;

typedef struct 
{
    int model_type;
    // Model A configuration
    std::string rknn_path_a;
    float nms_threshold_a;
    float box_threshold_a;
    int model_size_a;
    int class_num_a;
    // Model B configuration
    std::string rknn_path_b;
    float nms_threshold_b;
    float box_threshold_b;
    int model_size_b;
    int class_num_b;
} RKNN_CONFIG;

class DetectionApp {
public:
    // Constructor with two model info structures
    DetectionApp(detection_app_info info_a, detection_app_info info_b, 
                 bool is_sync, int frame_rate, size_t num_thread);

    DetectionApp(bool is_sync, int frame_rate, size_t num_thread);

    ~DetectionApp();

    void set_infer(ImagePtr image_data, size_t num);
    Objects get_infer(size_t num);

    input_size get_input_shape(size_t num=0);

    RKNN_CONFIG m_rknn_config;

private:
    const bool m_is_sync;
    const size_t m_num_thread;
    std::atomic<bool> m_is_opened {false};

    void readConfig();
    void Init(detection_app_info info_a, detection_app_info info_b, int frame_rate);
    Objects InferAsync(ObjectDetection& detector, ImagePtr image_data);
    void InferThread(size_t thread_num, int frame_rate);

    // Helper function to get model index based on camera index
    inline size_t getModelIndex(size_t camIdx) const {
        // camIdx 0,2 -> Model A (index 0,1)
        // camIdx 1,3 -> Model B (index 2,3)
        return (camIdx == 0 || camIdx == 2) ? camIdx : camIdx - 1 + 2;
    }

    std::vector<std::future<Objects>> m_asyncs;

    std::vector<std::unique_ptr<std::mutex>> m_mutexes;
    std::vector<bool> m_image_init;
    //std::vector<std::pair<Time::TimePoint, ImagePtr>> m_images;
    std::vector<std::pair<Time::TimePoint, std::vector<uint8_t>>> m_images;
    std::vector<Objects> m_objects;
    std::vector<std::thread> m_threads;

    std::vector<std::unique_ptr<ObjectDetection>> m_detectors;
};

#endif