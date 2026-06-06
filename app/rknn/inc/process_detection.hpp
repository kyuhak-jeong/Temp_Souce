#ifndef PROCESS_DETECTION_HPP_
#define PROCESS_DETECTION_HPP_

#include "app_vars.h"
#include "constants.h"
#include "detection.hpp"

#ifdef USE_SVM
    #define CROP_TOP            (int) (0)
    #define CROP_BOT            (int) (0)
    #define CROP_LEFT           (int) (192)
    #define CROP_RIGHT          (int) (192)
#else
    #define CROP_TOP            (int) (0)
    #define CROP_BOT            (int) (0)
    #define CROP_LEFT           (int) (0)
    #define CROP_RIGHT          (int) (0)
#endif

void init_process_detection();
void deinit_process_detection();
void process_callback_data_for_inference(uint8_t* image_data, int camIdx);

#endif
