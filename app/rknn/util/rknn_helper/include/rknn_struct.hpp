#ifndef RKNN_STRUCT_HPP_
#define RKNN_STRUCT_HPP_

#include <iostream>

#include "rknn_api.h"

namespace RknnHelper
{

typedef struct {
    int32_t qp_zp;
    float qp_scale;
    rknn_tensor_qnt_type qp_type; 
} rknn_quant_info_t;

typedef struct {
    rknn_tensor_type type;
    rknn_tensor_format order;
} rknn_format_t;

typedef struct {
    uint32_t features;
    uint32_t height;
    uint32_t width;
} rknn_3d_image_shape_t;

typedef struct {
    char network_name[RKNN_MAX_NAME_LEN];
    size_t size;
    rknn_format_t format;
    rknn_3d_image_shape_t shape;
    rknn_quant_info_t quant_info;
    rknn_tensor_attr attr;
} rknn_stream_info_t;

}

#endif