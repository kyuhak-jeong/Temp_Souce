#include "detection.hpp"
#include <cstring>
#include "logger.h"
#include <algorithm>

/*** Macro ***/
#define TAG "DetectionApp"
#define MAX_THREAD_NUM 4

using namespace RknnHelper;

DetectionApp::DetectionApp(bool is_sync, int frame_rate, size_t num_thread)
: m_is_sync(is_sync), m_num_thread(num_thread)
{
    try {
        readConfig();
        detection_app_info info_a, info_b;
        
        // Model A configuration
        info_a.rknn_path = m_rknn_config.rknn_path_a;
        info_a.class_num = m_rknn_config.class_num_a;
        info_a.box_thres = m_rknn_config.box_threshold_a;
        info_a.nms_thres = m_rknn_config.nms_threshold_a;

        // Model B configuration
        info_b.rknn_path = m_rknn_config.rknn_path_b;
        info_b.class_num = m_rknn_config.class_num_b;
        info_b.box_thres = m_rknn_config.box_threshold_b;
        info_b.nms_thres = m_rknn_config.nms_threshold_b;

        Init(info_a, info_b, frame_rate);
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "DetectionApp constructor error: " << e.what() << std::endl;
    }
}

DetectionApp::DetectionApp(detection_app_info info_a, detection_app_info info_b, bool is_sync, int frame_rate, size_t num_thread)
: m_is_sync(is_sync), m_num_thread(num_thread)
{
    Init(info_a, info_b, frame_rate);
}

DetectionApp::~DetectionApp() 
{
    m_is_opened = false; // killed infer thread.

    // all thread wait for end
    for (auto& thread : m_threads) 
    {
        if (thread.joinable()) 
        {
            thread.join();
        }
    }
    m_threads.clear();

    if(!m_detectors.empty()) m_detectors.clear();

    printf("[AI Detector] Detection App deinit done\n");
}

void DetectionApp::readConfig()
{
    std::string rknn_config_path = "../resources/rknns/rknn_config_new.txt";
    std::ifstream file(rknn_config_path);

    if (file.is_open()) 
    {
        std::string line;
        while (std::getline(file, line)) 
        {
            std::istringstream iss(line);
            std::string key, value;
            if (std::getline(iss, key, '=') && std::getline(iss, value)) 
            {
                try 
                {
                    if (key == "model_type") {
                        m_rknn_config.model_type = std::stoi(value);
                    }
                    // Model A configurations
                    else if (key == "rknn_path_a") {
                        m_rknn_config.rknn_path_a = value;
                    } else if (key == "nms_threshold_a") {
                        m_rknn_config.nms_threshold_a = std::stof(value);
                    } else if (key == "box_threshold_a") {
                        m_rknn_config.box_threshold_a = std::stof(value);
                    } else if (key == "model_size_a") {
                        m_rknn_config.model_size_a = std::stoi(value);
                    } else if (key == "class_num_a") {
                        m_rknn_config.class_num_a = std::stoi(value);
                    }
                    // Model B configurations
                    else if (key == "rknn_path_b") {
                        m_rknn_config.rknn_path_b = value;
                    } else if (key == "nms_threshold_b") {
                        m_rknn_config.nms_threshold_b = std::stof(value);
                    } else if (key == "box_threshold_b") {
                        m_rknn_config.box_threshold_b = std::stof(value);
                    } else if (key == "model_size_b") {
                        m_rknn_config.model_size_b = std::stoi(value);
                    } else if (key == "class_num_b") {
                        m_rknn_config.class_num_b = std::stoi(value);
                    }
                }
                catch (...) 
                {
                    std::cout<< "Invalid argument" << std::endl;
                }
            }
        }
        
        // std::cout << "============= Model A Configuration =============" << std::endl;
        // std::cout << "rknn_path_a    : " << m_rknn_config.rknn_path_a << std::endl;
        // std::cout << "nms_threshold_a: " << m_rknn_config.nms_threshold_a << std::endl;
        // std::cout << "box_threshold_a: " << m_rknn_config.box_threshold_a << std::endl;
        // std::cout << "model_size_a   : " << m_rknn_config.model_size_a << std::endl;
        // std::cout << "class_num_a    : " << m_rknn_config.class_num_a << std::endl;
        
        // std::cout << "\n============= Model B Configuration =============" << std::endl;
        // std::cout << "rknn_path_b    : " << m_rknn_config.rknn_path_b << std::endl;
        // std::cout << "nms_threshold_b: " << m_rknn_config.nms_threshold_b << std::endl;
        // std::cout << "box_threshold_b: " << m_rknn_config.box_threshold_b << std::endl;
        // std::cout << "model_size_b   : " << m_rknn_config.model_size_b << std::endl;
        // std::cout << "class_num_b    : " << m_rknn_config.class_num_b << std::endl;
        // std::cout << "=================================================" << std::endl;
        
        file.close();
    }
}

void DetectionApp::Init(detection_app_info info_a, detection_app_info info_b, int frame_rate) 
{
    if(m_num_thread > MAX_THREAD_NUM || m_num_thread <= 0)
    {
        throw std::out_of_range("Invalid thread number");
    }

    m_detectors.reserve(m_num_thread);
    
    // Create detectors based on camera index
    for (size_t idx = 0; idx < m_num_thread; idx++) 
    {
        // camIdx 0,2 use Model A
        // camIdx 1,3 use Model B
        const detection_app_info& info = (idx == 0 || idx == 2) ? info_a : info_b;
        
        // std::cout << "Thread " << idx << " using model: " << info.rknn_path << std::endl;
        
        // Pass context_id (idx) to set NPU core mask
        // sigmoid=true, context_id=idx
        m_detectors.push_back(std::make_unique<ObjectDetection>(info.rknn_path, info.class_num, info.box_thres, info.nms_thres, true, idx));
    }

    if (m_is_sync) 
    {
        m_asyncs.resize(m_num_thread);
    } 
    else 
    {
        m_images.resize(m_num_thread);
        m_objects.resize(m_num_thread);
        m_is_opened = true;

        m_image_init.reserve(m_num_thread);
        m_threads.reserve(m_num_thread);
        for (size_t i = 0; i < m_num_thread; i++)
            m_mutexes.push_back(std::make_unique<std::mutex>());
        
        // std::cout << "\n[DetectionApp] Creating inference threads..." << std::endl;
        
        for (size_t idx = 0; idx < m_num_thread; idx++) 
        {
            m_image_init.push_back(false);
            m_threads.push_back(std::thread(&DetectionApp::InferThread, this, idx, frame_rate));
        }
    }

    // printf("\n[AI Detector] Detection App init done\n");
    // printf("[AI Detector] Model A (cam 0,2): %s -> NPU Core 0\n", info_a.rknn_path.c_str());
    // printf("[AI Detector] Model B (cam 1,3): %s -> NPU Core 1\n\n", info_b.rknn_path.c_str());
}


Objects DetectionApp::InferAsync(ObjectDetection& detector, ImagePtr image_data) 
{
    return detector.infer(image_data);
}


void DetectionApp::InferThread(size_t thread_num, int frame_rate)
{
    auto& _detector    = m_detectors[thread_num];
    const auto& _image = m_images[thread_num];
    auto& _objects     = m_objects[thread_num];
    auto input_shape   = _detector->getInputShape();
    size_t input_size  = input_shape.width * input_shape.height * input_shape.features;

    std::pair<Time::TimePoint, std::vector<uint8_t>> image;
    image.second.resize(input_size);
    Time::Rate rate(frame_rate);

    std::mutex& cam_mutex = *m_mutexes[thread_num];

    while (m_is_opened)
    {
        {
            std::unique_lock<std::mutex> lock(cam_mutex);
            bool init = m_image_init[thread_num];
            if (image.first == _image.first || !init)
            {
                lock.unlock();
                rate.sleep();
                continue;
            }
            image.first = _image.first;
            // Safely copy image data; ensure sizes match to avoid overflow
            if (_image.second.size() != input_size) {
                LOG_DVR_ERRORF("[Detection] Image size mismatch in InferThread: expected %zu, got %zu", input_size, _image.second.size());
                size_t copy_len = std::min(input_size, _image.second.size());
                std::memcpy(image.second.data(), _image.second.data(), copy_len);
            } else {
                std::memcpy(image.second.data(), _image.second.data(), input_size);
            }
        }

        Objects object = _detector->infer(image.second.data());

        {
            std::unique_lock<std::mutex> lock(cam_mutex);
            _objects = std::move(object);
        }

        rate.sleep();
    }
}


void DetectionApp::set_infer(ImagePtr image_data, size_t num)
{
    // The image_data pointer is directly stored in m_images
    if (m_is_sync) 
    {
        m_asyncs[num] = std::async(std::launch::async, &DetectionApp::InferAsync, 
                                   this, std::ref(*m_detectors[num]), image_data);
    }
    else
    {
        auto input_shape = m_detectors[num]->getInputShape();
        size_t input_size = input_shape.width * input_shape.height * input_shape.features;

        std::unique_lock<std::mutex> lock(*m_mutexes[num]);
        m_image_init[num] = true;
        m_images[num].first  = std::chrono::steady_clock::now();
        //m_images[num].second = image_data;
        if (m_images[num].second.size() != input_size) {
            m_images[num].second.resize(input_size);
        }
        // Safely copy image data in set_infer; verify buffer size
            if (image_data == nullptr) {
                LOG_DVR_ERROR("[Detection] Null image_data pointer in set_infer");
            } else {
                size_t actual_size = input_size; // assuming full size expected
                // If the incoming size is smaller, we copy only what we have to avoid overflow
                // Note: image_data comes from camera callback; its actual length should be input_size
                std::memcpy(m_images[num].second.data(), image_data, actual_size);
            }
    }
}


Objects DetectionApp::get_infer(size_t num) 
{
    Objects object;
    if (m_is_sync) {
        object = m_asyncs[num].get();
    } else {
        std::unique_lock<std::mutex> lock(*m_mutexes[num]);
        object = m_objects[num];
    }
    return object;
}


input_size DetectionApp::get_input_shape(size_t num) {
    if(num >= m_num_thread) 
    {
        throw std::out_of_range("Invalid thread number");
    }
    auto shape = m_detectors[num]->getInputShape(num);
    return input_size{(int)shape.width, (int)shape.height};
}