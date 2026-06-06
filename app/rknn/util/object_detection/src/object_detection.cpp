#include "object_detection.hpp"

/*** Macro ***/
#define TAG "ObjectDetection"

using namespace RknnHelper;

ObjectDetection::ObjectDetection() : RknnProcesser(), m_sigmoid(0),
m_class_num(0), m_decode_size(0), m_box_thres(0), m_nms_thres(0) {}

ObjectDetection::ObjectDetection(std::string rknn_path, uint8_t class_num,
                                 float box_thres, float nms_thres, bool sigmoid, 
                                 int context_id, bool show_info)
 : RknnProcesser(rknn_path, context_id, show_info), m_sigmoid(sigmoid),
   m_class_num(class_num), m_decode_size((uint8_t)5 + class_num),
   m_box_thres(box_thres), m_nms_thres(nms_thres) { Init(); }

ObjectDetection::ObjectDetection(ObjectDetection&& other) noexcept
 : RknnProcesser(std::move(other)), m_sigmoid(other.m_sigmoid), m_class_num(other.m_class_num),
   m_decode_size(other.m_decode_size), m_box_thres(other.m_box_thres), m_nms_thres(other.m_nms_thres)
{
    m_model_outputs.resize(other.m_model_outputs.size());
    for (size_t idx=0; idx<other.m_model_outputs.size(); idx++) {
        m_model_outputs[idx].resize(other.m_model_outputs[idx].size());
    }
    other.m_model_outputs.clear();
}

ObjectDetection& ObjectDetection::operator=(ObjectDetection&& other) noexcept {
    if (this != &other) {
        RknnProcesser::operator=(std::move(other));

        m_model_outputs.resize(other.m_model_outputs.size());
        for (size_t idx=0; idx<other.m_model_outputs.size(); idx++) {
            m_model_outputs[idx].resize(other.m_model_outputs[idx].size());
        }
        other.m_model_outputs.clear();
    }

    return *this;
}

void ObjectDetection::Init() {
    m_model_outputs.resize(RknnProcesser::getOutputBatch());
    for (size_t idx=0; idx<RknnProcesser::getOutputBatch(); idx++) {
        m_model_outputs[idx].resize(RknnProcesser::getOutputBatch(idx));
    }
}

// syan
float ObjectDetection::CalculateIOU(float xmin0, float ymin0, float xmax0, float ymax0,
                                    float xmin1, float ymin1, float xmax1, float ymax1)
{
    float w = fmax(0.f, fmin(xmax0, xmax1) - fmax(xmin0, xmin1));
    float h = fmax(0.f, fmin(ymax0, ymax1) - fmax(ymin0, ymin1));
    float i = w * h;
    float u = (xmax0 - xmin0) * (ymax0 - ymin0) + (xmax1 - xmin1) * (ymax1 - ymin1) - i;
    return u <= 0.f ? 0.f : (i / u);
}

// syan
int ObjectDetection::NMS(std::vector<Object>& src, std::vector<Object>& dst, int filter_id, float threshold) {
    size_t count = src.size();
    for (size_t i = 0; i < count; ++i) {
        if (src[i].class_id == -1 || src[i].class_id != filter_id) {
            continue;
        }
        
        Object best_object = src[i];
        for (size_t j = i + 1; j < count; ++j) {
            if (src[j].class_id == -1 || src[j].class_id != filter_id) {
                continue;
            }

            float xmin0 = src[i].box.x;
            float ymin0 = src[i].box.y;
            float xmax0 = xmin0 + src[i].box.width;
            float ymax0 = ymin0 + src[i].box.height;

            float xmin1 = src[j].box.x;
            float ymin1 = src[j].box.y;
            float xmax1 = xmin1 + src[j].box.width;
            float ymax1 = ymin1 + src[j].box.height;

            float iou = CalculateIOU(xmin0, ymin0, xmax0, ymax0, xmin1, ymin1, xmax1, ymax1);

            if (iou > threshold) {
                if (src[j].prob > best_object.prob) {
                    best_object = src[j];
                }

                src[j].class_id = -1;
            }
        }

        dst.push_back(best_object);
    }

    return 0;
}

void ObjectDetection::DecodeYolov5(std::vector<int8_t>& input, std::vector<Object>& objects,
                                   std::set<int>& class_id, float threshold, int* anchor,
                                   int grid_h, int grid_w, int stride, rknn_quant_info_t qp, bool sigmoid)
{
    int grid_len = grid_h * grid_w;

    float thres = sigmoid ? UnSigmoid(threshold) : threshold;
    int8_t thres_i8 = Quant(thres, qp.qp_scale, qp.qp_zp);

    for (int a = 0; a < 3; a++) {
        for (int i = 0; i < grid_h; i++) {
            for (int j = 0; j < grid_w; j++) {
                int8_t box_probs_i8 = input[(m_decode_size * a + 4) * grid_len + i * grid_w + j];
                if (box_probs_i8 < thres_i8) {
                    continue;
                }

                size_t start_idx = (m_decode_size * a) * grid_len + i * grid_w + j;

                uint8_t max_id = 0;
                int8_t max_probs_i8 = input[start_idx + (5 * grid_len)];
                for (uint8_t k = 1; k < m_class_num; ++k) {
                    int8_t prob = input[start_idx + ((5 + k) * grid_len)];
                    if (prob > max_probs_i8) {
                        max_id       = k;
                        max_probs_i8 = prob;
                    }
                }

                if (max_probs_i8 < thres_i8) {
                    continue;
                }

                float max_probs = DeQuant(max_probs_i8, qp.qp_scale, qp.qp_zp);
                float box_probs = DeQuant(box_probs_i8, qp.qp_scale, qp.qp_zp);

                if (sigmoid) {        
                    max_probs = Sigmoid(max_probs);
                    box_probs = Sigmoid(box_probs);
                }

                float prob = max_probs * box_probs;
                if (prob < threshold) {
                    continue;
                }

                float box_x = DeQuant(input[start_idx], qp.qp_scale, qp.qp_zp);
                float box_y = DeQuant(input[start_idx + grid_len], qp.qp_scale, qp.qp_zp);
                float box_w = DeQuant(input[start_idx + (2 * grid_len)], qp.qp_scale, qp.qp_zp);
                float box_h = DeQuant(input[start_idx + (3 * grid_len)], qp.qp_scale, qp.qp_zp);

                if (sigmoid) {
                    box_x = Sigmoid(box_x);
                    box_y = Sigmoid(box_y);
                    box_w = Sigmoid(box_w);
                    box_h = Sigmoid(box_h);
                }

                box_x = box_x * 2.0 - 0.5;
                box_y = box_y * 2.0 - 0.5;
                box_w = box_w * 2.0;
                box_h = box_h * 2.0;

                box_x = (box_x + j) * (float)stride;
                box_y = (box_y + i) * (float)stride;
                box_w = box_w * box_w * (float)anchor[a * 2];
                box_h = box_h * box_h * (float)anchor[a * 2 + 1];

                box_x -= (box_w / 2.0);
                box_y -= (box_h / 2.0);

                Box box = { box_x, box_y, box_w, box_h };
                Object object = { max_id, prob, box };
                objects.push_back(object);

                class_id.insert(max_id);
            }
        }
    }
}

std::vector<Object> ObjectDetection::infer(uint8_t* image_data) {
    RknnProcesser::infer(image_data, m_model_outputs);

    float model_in_w = getInputShape().width;
    float model_in_h = getInputShape().height;

    std::set<int> class_id;
    std::vector<Object> objects;

    // stride 8
    int stride0      = 8;
    int grid_h0      = model_in_h / stride0;
    int grid_w0      = model_in_w / stride0;
    size_t idx       = 0;
    DecodeYolov5(m_model_outputs[idx], objects, class_id, m_box_thres,
                 (int*)m_anchor0, grid_h0, grid_w0, stride0, getOutputQuant(idx), m_sigmoid);

    // stride 16
    int stride1      = 16;
    int grid_h1      = model_in_h / stride1;
    int grid_w1      = model_in_w / stride1;
    idx++;
    DecodeYolov5(m_model_outputs[idx], objects, class_id, m_box_thres,
                 (int*)m_anchor1, grid_h1, grid_w1, stride1, getOutputQuant(idx), m_sigmoid);

    // stride 32
    int stride2      = 32;
    int grid_h2      = model_in_h / stride2;
    int grid_w2      = model_in_w / stride2;
    idx++;
    DecodeYolov5(m_model_outputs[idx], objects, class_id, m_box_thres,
                 (int*)m_anchor2, grid_h2, grid_w2, stride2, getOutputQuant(idx), m_sigmoid);

    std::vector<Object> nms_objects;
    for (auto cl : class_id) {
        NMS(objects, nms_objects, cl, m_nms_thres);
    }

    return nms_objects;
}