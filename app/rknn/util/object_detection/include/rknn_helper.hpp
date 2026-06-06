#ifndef RKNN_HELPER_HPP_
#define RKNN_HELPER_HPP_

/*** Include ***/
/* for general */
#include <iostream>
#include <string.h>
#include <vector>

/* for RKNN */
#include "rknn_api.h"
#include "rknn_struct.hpp"

namespace RknnHelper
{

inline static int32_t Clip(float val, float min, float max) {
    return val <= min ? min : (val >= max ? max : val);
}

inline static int8_t Quant(float value, float qp_scale, float qp_zp) {
    float dst_val = ((value / qp_scale) + qp_zp);
    return (int8_t)Clip(dst_val, -128.f, 127.f);
}

inline static float DeQuant(int8_t value, float qp_scale, float qp_zp) {
    return (float)(((float)value - qp_zp) * qp_scale);
}

class RknnProcesser {
public:
    RknnProcesser();
    RknnProcesser(std::string rknn_path, int context_id=0, bool show_info=true);
    RknnProcesser(const RknnProcesser&) = delete;
    RknnProcesser(RknnProcesser&& other) noexcept;

    ~RknnProcesser();

    RknnProcesser& operator=(const RknnProcesser&) = delete;
    RknnProcesser& operator=(RknnProcesser&& other) noexcept;

    bool infer(uint8_t* image_data, std::vector<std::vector<int8_t>>& model_outputs);
    rknn_3d_image_shape_t getInputShape(size_t idx=0);

protected:
    inline size_t getInputBatch() { return m_io_num.n_input; }
    inline size_t getOutputBatch() { return m_io_num.n_output; }
    size_t getOutputBatch(size_t idx);

    rknn_stream_info_t getOutputInfo(size_t idx=0);
    rknn_3d_image_shape_t getOutputShape(size_t idx=0);

    rknn_quant_info_t getOutputQuant(size_t idx=0);
    int8_t OutputQuant(float value, size_t idx=0);
    float OutputDeQuant(int8_t value, size_t idx=0);

private:
    static unsigned char* LoadData(FILE* fp, size_t ofst, size_t sz);
    static unsigned char* LoadModel(const char* filename, int* model_size);

    static rknn_stream_info_t ParsInfo(rknn_tensor_attr tensor_attr);

    bool Initialize(const std::string& rknn_path, int context_id=0);

    rknn_sdk_version m_sdk_version;

    rknn_context m_ctx;
    rknn_input_output_num m_io_num;
    
    std::vector<rknn_stream_info_t> m_input_stream_infos;
    std::vector<rknn_stream_info_t> m_output_stream_infos;

    std::vector<rknn_tensor_mem*> m_input_streams;
    std::vector<rknn_tensor_mem*> m_output_streams;
};

}

#endif