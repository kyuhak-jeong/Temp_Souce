#ifndef RKNN_INFOS_HPP_
#define RKNN_INFOS_HPP_

/*** Include ***/
/* for general */
#include <iostream>

/* for rknn */
//#include <hailo/hailort.hpp>

namespace RknnHelper
{

inline static void print_qnt_type(rknn_tensor_qnt_type type){
    switch (type)
    {
    case RKNN_TENSOR_QNT_NONE:
        std::cout << "quant_info.qp_type  : NONE" << std::endl;
        break;
    
    case RKNN_TENSOR_QNT_DFP:
        std::cout << "quant_info.qp_type  : DFP" << std::endl;
        break;
    
    case RKNN_TENSOR_QNT_AFFINE_ASYMMETRIC:
        std::cout << "quant_info.qp_type  : AFFINE" << std::endl;
        break;
    
    default:
        std::cout << "quant_info.qp_type  : UNKNOW" << std::endl;
        break;
    }
}

inline static void print_format_order(rknn_tensor_format order){
    switch (order)
    {
    case RKNN_TENSOR_NCHW:
        std::cout << "format.order : NCHW, [N, C, H, W]" << std::endl;
        break;
    
    case RKNN_TENSOR_NHWC:
        std::cout << "format.order : NHWC, [N, H, W, C]" << std::endl;
        break;
    
    case RKNN_TENSOR_NC1HWC2:
        std::cout << "format.order : NC1HWC2, [N, C, H, W, C]" << std::endl;
        break;
    
    case RKNN_TENSOR_UNDEFINED:
        std::cout << "format.order : UNDEFINED" << std::endl;
        break;
    
    default:
        std::cout << "format.order : UNKNOW" << std::endl;
        break;
    }
}

inline void print_format_type(rknn_tensor_type type){
    switch (type)
    {
    case RKNN_TENSOR_FLOAT32:
        std::cout << "format.type  : FLOAT32" << std::endl;
        break;
    
    case RKNN_TENSOR_FLOAT16:
        std::cout << "format.type  : FLOAT16" << std::endl;
        break;
    
    case RKNN_TENSOR_INT8:
        std::cout << "format.type  : INT8" << std::endl;
        break;
    
    case RKNN_TENSOR_UINT8:
        std::cout << "format.type  : UINT8" << std::endl;
        break;
    
    case RKNN_TENSOR_INT16:
        std::cout << "format.type  : INT16" << std::endl;
        break;
    
    case RKNN_TENSOR_UINT16:
        std::cout << "format.type  : UINT16" << std::endl;
        break;
    
    case RKNN_TENSOR_INT32:
        std::cout << "format.type  : INT32" << std::endl;
        break;
    
    case RKNN_TENSOR_UINT32:
        std::cout << "format.type  : UINT32" << std::endl;
        break;
    
    case RKNN_TENSOR_INT64:
        std::cout << "format.type  : INT64" << std::endl;
        break;
    
    case RKNN_TENSOR_BOOL:
        std::cout << "format.type  : BOOL" << std::endl;
        break;
    
    default:
        std::cout << "format.type : UNKNOW" << std::endl;
        break;
    }
}

inline static void print_stream_info(std::vector<rknn_stream_info_t> infos) {
    size_t count = 0;
    for (auto info : infos) {
        std::cout << "======== batch " << count  << " =========" << std::endl;
        std::cout << "network_name : " << info.network_name << std::endl;
        print_format_order(info.format.order);
        print_format_type(info.format.type);
        std::cout << "shape.features : " << info.shape.features << std::endl;
        std::cout << "shape.height   : " << info.shape.height << std::endl;
        std::cout << "shape.width    : " << info.shape.width << std::endl;
        std::cout << "quant_info.qp_zp    : " << info.quant_info.qp_zp << std::endl;
        std::cout << "quant_info.qp_scale : " << info.quant_info.qp_scale << std::endl;
        print_qnt_type(info.quant_info.qp_type);
        count++;
    }
}

}

#endif