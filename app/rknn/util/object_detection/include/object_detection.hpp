#ifndef OBJECT_DETECTION_HPP_
#define OBJECT_DETECTION_HPP_

/*** Include ***/
/* for general */
#include <iostream>
#include <set>
#include <cmath>

/* for rknn */
#include "rknn_helper.hpp"

typedef struct {
    float x;
    float y;
    float width;
    float height;
} Box;

typedef struct {
    int   class_id;
    float prob;
    Box   box;
} Object;

class ObjectDetection : RknnHelper::RknnProcesser {
public:
    ObjectDetection();
    ObjectDetection(std::string rknn_path, uint8_t class_num,
                    float box_thres=0.25, float nms_thres=0.45,
                    bool sigmoid=true, int context_id=0, bool show_info=true);
    ObjectDetection(const ObjectDetection&) = delete;
    ObjectDetection(ObjectDetection&& other) noexcept;

    ObjectDetection& operator=(const ObjectDetection&) = delete;
    ObjectDetection& operator=(ObjectDetection&& other) noexcept;

    std::vector<Object> infer(uint8_t* image_data);
    RknnHelper::rknn_3d_image_shape_t getInputShape(size_t idx=0) {
        return RknnHelper::RknnProcesser::getInputShape(idx);
    };
    
private:
    inline static float Clamp(float val, float min, float max) { return val > min ? (val < max ? val : max) : min; }

    inline static float Sigmoid(float x) { return 1.0 / (1.0 + expf(-x)); }
    inline static float UnSigmoid(float y) { return std::log(y / (1.0 - y)); }

    void Init();

    static float CalculateIOU(float xmin0, float ymin0, float xmax0, float ymax0,
                              float xmin1, float ymin1, float xmax1, float ymax1);

    static int NMS(std::vector<Object>& src, std::vector<Object>& dst, int filter_id, float threshold);

    void DecodeYolov5(std::vector<int8_t>& input, std::vector<Object>& objects,
                      std::set<int>& class_id, float threshold, int* anchor,
                      int grid_h, int grid_w, int stride, RknnHelper::rknn_quant_info_t qp, bool sigmoid=true);

    const bool m_sigmoid;

    const uint8_t m_class_num;
    const uint8_t m_decode_size;

    const float m_box_thres;
    const float m_nms_thres;

    const int m_anchor0[6] = {10, 13, 16, 30, 33, 23};
    const int m_anchor1[6] = {30, 61, 62, 45, 59, 119};
    const int m_anchor2[6] = {116, 90, 156, 198, 373, 326};

    std::vector<std::vector<int8_t>> m_model_outputs;
};

#endif