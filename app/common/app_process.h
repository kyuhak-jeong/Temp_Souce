/*********************************************************************
*   app_process.h
*
*   Copyright 2023 SkyAutoNet.
*
*   All Rights Reserved
*
**********************************************************************/

#ifndef __APP_PROCESS_H__
#define __APP_PROCESS_H__

#include "constants.h"
#include <smallwin.hpp>

typedef enum
{
    MAIN_PROCESS = 0,

    CLEANUP_PROCESS = 1,

    GST_CAMERA_PROCESS = 2,

    SVM_UI_PROCESS = 3,

    CALIB_UI_PROCESS = 4,

    AICAM_UI_PROCESS = 5,

	PROCESS_ID_END
    
} APP_PROCESS_ID;

extern APP_PROCESS_ID backup_process_id;

#ifdef __cplusplus
    extern "C" 
    {
#endif

        #ifdef __cplusplus
            extern void open_process_id(APP_PROCESS_ID screen_id);
        #else
            extern void open_process_id(APP_PROCESS_ID screen_id);
        #endif

#ifdef __cplusplus
    }
#endif


#endif // __APP_PROCESS_H__

