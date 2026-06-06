#include "RgaUtils.h"
#include "RgaApi.h"
#include "drmrga.h"
#include "im2d.h"
#include "rga.h"

#include "process_detection.hpp"

#define DEBUG 0

DetectionApp* m_pDetection = nullptr;

bool   infer_sync  = false;
int    frame_rate  = 15;
size_t thread_size = 4;

void init_process_detection()
{
    std::cout << "\n------------------Init_AI_Detector------------------" << std::endl;

    if (m_pDetection == nullptr)
    {
        m_pDetection = new DetectionApp(infer_sync, frame_rate, thread_size);
        std::cout << "------------------Init_AI_Detector_done------------------" << std::endl;
    }
}

void deinit_process_detection()
{
    std::cout << "\n------------------Deinit_AI_Detector------------------" << std::endl;

    if (m_pDetection != nullptr)
    {
        delete m_pDetection;
        std::cout << "------------------Deinit_AI_Detector_done------------------" << std::endl;
    }

    m_pDetection = nullptr;
}

void ConvertNV12toRGB(uint8_t*      image_data,
                      int           y_stride,
                      uint8_t*      rgb_data,
                      int           camIdx,
                      DetectionApp* detector)
{
    rga_info_t src, dst;
    memset(&src, 0, sizeof(src));
    memset(&dst, 0, sizeof(dst));

    src.virAddr = image_data;
    dst.virAddr = rgb_data;

    src.format = RK_FORMAT_YCbCr_420_SP;
    dst.format = RK_FORMAT_RGB_888;

    int model_size = (camIdx == 0 || camIdx == 2)
                   ? detector->m_rknn_config.model_size_a
                   : detector->m_rknn_config.model_size_b;

    rga_set_rect(&src.rect,
                 CROP_LEFT, CROP_TOP,
                 IMG_WIDTH  - (CROP_LEFT + CROP_RIGHT),
                 IMG_HEIGHT - (CROP_TOP  + CROP_BOT),
                 y_stride, IMG_HEIGHT, src.format);
    rga_set_rect(&dst.rect, 0, 0, model_size, model_size, model_size, model_size, dst.format);

    int ret = c_RkRgaBlit(&src, &dst, nullptr);
    if (ret)
        std::cerr << "[AI Detector] c_RkRgaBlit failed: " << strerror(errno) << std::endl;
}

void ConvertNV12toRGBwithBufferHandler(uint8_t*      image_data,
                                       uint8_t*      rgb_data,
                                       int           camIdx,
                                       DetectionApp* detector)
{
    rga_buffer_t        src_img, crop_img, resized_img, dst_img;
    rga_buffer_handle_t src_handle = 0, crop_handle = 0,
                        resized_handle = 0, dst_handle = 0;
    IM_STATUS ret;

    const int src_format = RK_FORMAT_YCbCr_420_SP;
    const int dst_format = RK_FORMAT_RGB_888;

    const int model_size = (camIdx == 0 || camIdx == 2)
                         ? detector->m_rknn_config.model_size_a
                         : detector->m_rknn_config.model_size_b;

    const int crop_w = IMG_WIDTH  - (CROP_LEFT + CROP_RIGHT);
    const int crop_h = IMG_HEIGHT - (CROP_TOP  + CROP_BOT);

    const int crop_buf_size    = crop_w * crop_h            * get_bpp_from_format(src_format);
    const int resized_buf_size = model_size * model_size    * get_bpp_from_format(src_format);
    const int dst_buf_size     = model_size * model_size    * get_bpp_from_format(dst_format);

    uint8_t* crop_buf    = static_cast<uint8_t*>(malloc(crop_buf_size));
    uint8_t* resized_buf = static_cast<uint8_t*>(malloc(resized_buf_size));

    if (!crop_buf || !resized_buf)
    {
        printf("[AI_Detector] malloc failed\n");
        if (crop_buf)    free(crop_buf);
        if (resized_buf) free(resized_buf);
        return;
    }

    src_handle     = importbuffer_virtualaddr(image_data, IMG_WIDTH * IMG_HEIGHT * get_bpp_from_format(src_format));
    crop_handle    = importbuffer_virtualaddr(crop_buf,    crop_buf_size);
    resized_handle = importbuffer_virtualaddr(resized_buf, resized_buf_size);
    dst_handle     = importbuffer_virtualaddr(rgb_data,    dst_buf_size);

    if (!src_handle || !crop_handle || !resized_handle || !dst_handle)
    {
        printf("[AI_Detector] Importbuffer failed!\n");
        goto cleanup;
    }

    src_img     = wrapbuffer_handle(src_handle,     IMG_WIDTH, IMG_HEIGHT, src_format);
    crop_img    = wrapbuffer_handle(crop_handle,    crop_w,    crop_h,     src_format);
    resized_img = wrapbuffer_handle(resized_handle, model_size, model_size, src_format);
    dst_img     = wrapbuffer_handle(dst_handle,     model_size, model_size, dst_format);

    {
        im_rect crop_rect = { CROP_LEFT, CROP_TOP, crop_w, crop_h };

        ret = imcrop(src_img, crop_img, crop_rect);
        if (ret != IM_STATUS_SUCCESS)
            printf("[AI_Detector] imcrop error: %s\n", imStrError(ret));

        ret = imresize(crop_img, dst_img);
        if (ret != IM_STATUS_SUCCESS)
            printf("[AI_Detector] imresize error: %s\n", imStrError(ret));
    }

cleanup:
    if (src_handle != 0)        releasebuffer_handle(src_handle);
    if (crop_handle != 0)       releasebuffer_handle(crop_handle);
    if (resized_handle != 0)    releasebuffer_handle(resized_handle);
    if (dst_handle != 0)        releasebuffer_handle(dst_handle);
    if (crop_buf != nullptr)    { free(crop_buf);    crop_buf    = nullptr; }
    if (resized_buf != nullptr) { free(resized_buf); resized_buf = nullptr; }
}

void process_callback_data_for_inference(uint8_t* image_data, int camIdx)
{
    if (m_pDetection == nullptr) return;

    const int model_size = (camIdx == 0 || camIdx == 2)
                         ? m_pDetection->m_rknn_config.model_size_a
                         : m_pDetection->m_rknn_config.model_size_b;

    m_pDetection->set_infer(image_data, camIdx);
    auto objects = m_pDetection->get_infer(camIdx);

    // -- Coordinate scale factors (model space → original image pixels) -------
    const float crop_w    = static_cast<float>(IMG_WIDTH  - (CROP_LEFT + CROP_RIGHT));
    const float crop_h    = static_cast<float>(IMG_HEIGHT - (CROP_TOP  + CROP_BOT));
    const float scale_x   = crop_w / static_cast<float>(model_size);
    const float scale_y   = crop_h / static_cast<float>(model_size);
    const float inv_img_w = 1.0f   / static_cast<float>(IMG_WIDTH);
    const float inv_img_h = 1.0f   / static_cast<float>(IMG_HEIGHT);

    // -- Build output vector --------------------------------------------------
    APP::detected_objs[camIdx].clear();

    for (const auto& object : objects)
    {
        // Reject near-full-frame detections (likely background false positives).
        if (object.box.width >= model_size * 0.8f) continue;

        const APP::AI::DomainClass* cls = APP::AI::getByClassId(object.class_id);
        if (cls == nullptr) continue;

        // ── Convert box: model coords → pixel space → normalised [0,1] ───────
        const float nx = (CROP_LEFT + object.box.x     * scale_x) * inv_img_w;
        const float ny = (CROP_TOP  + object.box.y     * scale_y) * inv_img_h;
        const float nw = (           object.box.width  * scale_x) * inv_img_w;
        const float nh = (           object.box.height * scale_y) * inv_img_h;

        APP::detected_objs[camIdx].emplace_back(nx, ny, nw, nh,
            object.class_id, cls->domainId, cls->color, cls->label, object.prob);

        #if DEBUG
            printf("[AI Detector] cam=%d class=%d → domain=%d(%s) conf=%.2f  norm(%.3f,%.3f,%.3f,%.3f)\n",
               camIdx, object.class_id, cls->domainId, cls->label, object.prob, nx, ny, nw, nh);
        #endif
    }
}
