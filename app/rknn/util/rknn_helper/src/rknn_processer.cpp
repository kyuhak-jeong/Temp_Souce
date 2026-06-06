#include "rknn_helper.hpp"
#include "rknn_infos.hpp"

/*** Macro ***/
#define TAG "RknnProcesser"

using namespace RknnHelper;

RknnProcesser::RknnProcesser() 
{
    memset(&m_sdk_version, 0, sizeof(rknn_sdk_version));
    memset(&m_ctx, 0, sizeof(rknn_context));
    memset(&m_io_num, 0, sizeof(rknn_input_output_num));
}

RknnProcesser::RknnProcesser(std::string rknn_path, int context_id, bool show_info) 
{
    Initialize(rknn_path, context_id); // Initialize with context_id for NPU core assignment
}

RknnProcesser::RknnProcesser(RknnProcesser&& other) noexcept {
    strcpy(m_sdk_version.api_version, other.m_sdk_version.api_version);
    strcpy(m_sdk_version.drv_version, other.m_sdk_version.drv_version);

    m_ctx = other.m_ctx;
    m_io_num.n_input = other.m_io_num.n_input;
    m_io_num.n_output = other.m_io_num.n_output;

    m_input_stream_infos = other.m_input_stream_infos;
    m_output_stream_infos = other.m_output_stream_infos;

    m_input_streams = other.m_input_streams;
    m_output_streams = other.m_output_streams;

    memset(&other.m_sdk_version, 0, sizeof(rknn_sdk_version));
    memset(&other.m_ctx, 0, sizeof(rknn_context));
    memset(&other.m_io_num, 0, sizeof(rknn_input_output_num));
    other.m_input_stream_infos.clear();
    other.m_output_stream_infos.clear();
    other.m_input_streams.clear();
    other.m_output_streams.clear();
}

RknnProcesser::~RknnProcesser() {
    for (auto& input_stream : m_input_streams) {
        rknn_destroy_mem(m_ctx, input_stream);
    }

    for (auto& output_stream : m_output_streams) {
        rknn_destroy_mem(m_ctx, output_stream);
    }
 
    rknn_destroy(m_ctx);
}

RknnProcesser& RknnProcesser::operator=(RknnProcesser&& other) noexcept {
    if (this != &other) {
        strcpy(m_sdk_version.api_version, other.m_sdk_version.api_version);
        strcpy(m_sdk_version.drv_version, other.m_sdk_version.drv_version);

        m_ctx = other.m_ctx;
        m_io_num.n_input = other.m_io_num.n_input;
        m_io_num.n_output = other.m_io_num.n_output;

        m_input_stream_infos = other.m_input_stream_infos;
        m_output_stream_infos = other.m_output_stream_infos;

        m_input_streams = other.m_input_streams;
        m_output_streams = other.m_output_streams;

        memset(&other.m_sdk_version, 0, sizeof(rknn_sdk_version));
        memset(&other.m_ctx, 0, sizeof(rknn_context));
        memset(&other.m_io_num, 0, sizeof(rknn_input_output_num));
        other.m_input_stream_infos.clear();
        other.m_output_stream_infos.clear();
        other.m_input_streams.clear();
        other.m_output_streams.clear();
    }

    return *this;
}

unsigned char* RknnProcesser::LoadData(FILE* fp, size_t ofst, size_t sz) {
    unsigned char* data;
    int ret;

    data = NULL;

    if (NULL == fp) {
        return NULL;
    }

    ret = fseek(fp, ofst, SEEK_SET);
    if (ret != 0) {
        printf("blob seek failure.\n");
        return NULL;
    }

    data = (unsigned char*)malloc(sz);
    if (data == NULL) {
        printf("buffer malloc failure.\n");
        return NULL;
    }

    ret = fread(data, 1, sz, fp);

    return data;
}

unsigned char* RknnProcesser::LoadModel(const char* filename, int* model_size) {
    FILE* fp;
    unsigned char* data;

    fp = fopen(filename, "rb");
    if (NULL == fp) {
        printf("Open file %s failed.\n", filename);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);

    data = LoadData(fp, 0, size);

    fclose(fp);

    *model_size = size;

    return data;
}

rknn_stream_info_t RknnProcesser::ParsInfo(rknn_tensor_attr tensor_attr) {
    rknn_stream_info_t info;
    info.attr = tensor_attr;

    strcpy(info.network_name, tensor_attr.name);

    info.size = tensor_attr.n_elems;

    info.format.type = tensor_attr.type;
    info.format.order = tensor_attr.fmt;
    
    if (tensor_attr.fmt == RKNN_TENSOR_NCHW) {
        info.shape.features = tensor_attr.dims[1];
        info.shape.height   = tensor_attr.dims[2];
        info.shape.width    = tensor_attr.dims[3];
    } else if (tensor_attr.fmt == RKNN_TENSOR_NHWC) {
        info.shape.height   = tensor_attr.dims[1];
        info.shape.width    = tensor_attr.dims[2];
        info.shape.features = tensor_attr.dims[3];
    } else { 
        info.shape.features = 0;
        info.shape.height   = 0;
        info.shape.width    = 0;
        info.attr.fmt       = RKNN_TENSOR_UNDEFINED;
    }

    info.quant_info.qp_zp    = tensor_attr.zp;
    info.quant_info.qp_scale = tensor_attr.scale;
    info.quant_info.qp_type  = tensor_attr.qnt_type;

    return info;
}

bool RknnProcesser::Initialize(const std::string& rknn_path, int context_id) 
{
    int ret = -1;

    int model_data_size = 0;
    unsigned char* model_data = LoadModel(const_cast<char*>(rknn_path.c_str()), &model_data_size);
    if (model_data == NULL) {
        printf("load_model error: %s\n", rknn_path.c_str());
        return false;
    }

    ret = rknn_init(&m_ctx, model_data, model_data_size, 0, NULL);
    if (ret < 0) {
        printf("rknn_init error ret=%d\n", ret);
        return false;
    }

    if (model_data) {
        free(model_data);
    }

    ret = rknn_query(m_ctx, RKNN_QUERY_SDK_VERSION, &m_sdk_version, sizeof(rknn_sdk_version));
    if (ret < 0) {
        printf("rknn_query rknn_sdk_version error ret=%d\n", ret);
        return false;
    }

    ret = rknn_query(m_ctx, RKNN_QUERY_IN_OUT_NUM, &m_io_num, sizeof(rknn_input_output_num));
    if (ret < 0) {
        printf("rknn_init rknn_input_output_num error ret=%d\n", ret);
        return false;
    }

    for (int i = 0; i < m_io_num.n_input; i++) {
        rknn_tensor_attr input_attr;
        input_attr.index = i;
        
        ret = rknn_query(m_ctx, RKNN_QUERY_INPUT_ATTR, &input_attr, sizeof(rknn_tensor_attr));
        if (ret < 0) {
            printf("rknn_query RKNN_QUERY_INPUT_ATTR error ret=%d\n", ret);
            return false;
        }

        input_attr.type = RKNN_TENSOR_UINT8;

        rknn_stream_info_t info = ParsInfo(input_attr);
        
        rknn_tensor_mem* tensor_mem = rknn_create_mem(m_ctx, info.size);
        memset(tensor_mem->virt_addr, 0, info.size);

        ret = rknn_set_io_mem(m_ctx, tensor_mem, &info.attr);
        if (ret < 0) {
            printf("input rknn_set_io_mem error ret=%d\n", ret);
            return false;
        }

        m_input_streams.push_back(tensor_mem);
        m_input_stream_infos.push_back(info);
    }

    for (int i = 0; i < m_io_num.n_output; i++) 
    {
        rknn_tensor_attr output_attr;
        output_attr.index = i;

        ret = rknn_query(m_ctx, RKNN_QUERY_OUTPUT_ATTR, &output_attr, sizeof(rknn_tensor_attr));
        if (ret < 0) {
            printf("rknn_query RKNN_QUERY_OUTPUT_ATTR error ret=%d\n", ret);
            return false;
        }

        rknn_stream_info_t info = ParsInfo(output_attr);

        rknn_tensor_mem* tensor_mem = rknn_create_mem(m_ctx, info.size);
        memset(tensor_mem->virt_addr, 0, info.size);

        ret = rknn_set_io_mem(m_ctx, tensor_mem, &info.attr);
        if (ret < 0) {
            printf("input rknn_set_io_mem error ret=%d\n", ret);
            return false;
        }

        m_output_streams.push_back(tensor_mem);
        m_output_stream_infos.push_back(info);
    }

    // Set NPU core mask based on context_id
    // context_id 0, 2 -> NPU Core 0
    // context_id 1, 3 -> NPU Core 1
    rknn_core_mask core_mask;
    
    #ifdef DEVICE_RK3576 // RK3576: RKNN_NPU_CORE_0_1 (2 cores available)
        if (context_id == 0 || context_id == 2) 
        {
            core_mask = RKNN_NPU_CORE_0;
            printf("[RKNN Context %d] Using NPU Core 0\n", context_id);
        }
        else 
        {
            core_mask = RKNN_NPU_CORE_1;
            printf("[RKNN Context %d] Using NPU Core 1\n", context_id);
        }
    #else // RK3588: RKNN_NPU_CORE_0_1_2 (3 cores available)        
        switch(context_id)
        {
            case 0:
                core_mask = RKNN_NPU_CORE_0;
                printf("[RKNN Context %d] Using NPU Core 0\n", context_id);
                break;
            case 1:
            case 3:
                core_mask = RKNN_NPU_CORE_1;
                printf("[RKNN Context %d] Using NPU Core 1\n", context_id);
                break;
            case 4:
                core_mask = RKNN_NPU_CORE_2;
                printf("[RKNN Context %d] Using NPU Core 2\n", context_id);
                break;
            default:
                core_mask = RKNN_NPU_CORE_0_1_2;
                break;
        }
    #endif

    // if (context_id == 0)
    // {
    //     core_mask = RKNN_NPU_CORE_0;
    //     printf("[RKNN Context %d] Using NPU Core 0\n", context_id);
    // }
    // else if (context_id == 3 )
    // {  
    //     core_mask = RKNN_NPU_CORE_1;
    //     printf("[RKNN Context %d] Using NPU Core 1\n", context_id);
    // }
    // else
    // {
    //     core_mask = RKNN_NPU_CORE_2;
    //     printf("[RKNN Context %d] Using NPU Core 1\n", context_id);
    // }

    ret = rknn_set_core_mask(m_ctx, core_mask);
    if (ret < 0) {
        printf("[RKNN Context %d] rknn_set_core_mask error ret=%d\n", context_id, ret);
        return false;
    }
    
    printf("[RKNN Context %d] Successfully set core mask\n", context_id);

    return true;
}

bool RknnProcesser::infer(uint8_t* image_data, std::vector<std::vector<int8_t>>& model_outputs) {
    int ret = -1;

    for (size_t idx = 0; idx < getInputBatch(); idx++) {
        rknn_stream_info_t& info = m_input_stream_infos[idx];
        memcpy(m_input_streams[idx]->virt_addr, image_data, info.size);
    }

    ret = rknn_run(m_ctx, nullptr);
    if (ret < 0) {
        printf("rknn_run error ret=%d\n", ret);
        return false;
    }

    if (model_outputs.size() != m_io_num.n_output) {
        model_outputs.resize(m_io_num.n_output);
    }
    
    for (size_t idx = 0; idx < m_io_num.n_output; idx++) {
        rknn_stream_info_t& info = m_output_stream_infos[idx];

        if (model_outputs[idx].size() != info.size) {
            model_outputs[idx].resize(info.size);
        }

        memcpy(model_outputs[idx].data(), m_output_streams[idx]->virt_addr, info.size * sizeof(int8_t));
    }

    return true;
}

rknn_3d_image_shape_t RknnProcesser::getInputShape(size_t idx) {
    if (m_io_num.n_input > idx) {
        return m_input_stream_infos[idx].shape;
    } else {
        return rknn_3d_image_shape_t{0, 0, 0};
    }
}

size_t RknnProcesser::getOutputBatch(size_t idx) {
    if (m_io_num.n_output > idx) {
        rknn_stream_info_t& info = m_output_stream_infos[idx];
        return info.shape.width * info.shape.height * info.shape.features;
    } else {
        return 0;
    }
}

rknn_stream_info_t RknnProcesser::getOutputInfo(size_t idx) {
    if (m_io_num.n_output > idx) {
        return m_output_stream_infos[idx];
    } else {
        return m_output_stream_infos[0];
    }
}

rknn_3d_image_shape_t RknnProcesser::getOutputShape(size_t idx) {
    if (m_io_num.n_output > idx) {
        return m_output_stream_infos[idx].shape;
    } else {
        return rknn_3d_image_shape_t{0, 0, 0};
    }
}

rknn_quant_info_t RknnProcesser::getOutputQuant(size_t idx) {
    if (m_io_num.n_output > idx) {
        return m_output_stream_infos[idx].quant_info;
    } else {
        return rknn_quant_info_t{0, 0, RKNN_TENSOR_QNT_NONE};
    }
}

int8_t RknnProcesser::OutputQuant(float value, size_t idx) {
    if (m_io_num.n_output > idx) {
        rknn_quant_info_t quant_info = m_output_stream_infos[idx].quant_info;
        return Quant(value, quant_info.qp_scale, quant_info.qp_zp);
    } else {
        return 0;
    }
}

float RknnProcesser::OutputDeQuant(int8_t value, size_t idx) {
    if (m_io_num.n_output > idx) {
        rknn_quant_info_t quant_info = m_output_stream_infos[idx].quant_info;
        return DeQuant(value, quant_info.qp_scale, quant_info.qp_zp);
    } else {
        return 0;
    }
}