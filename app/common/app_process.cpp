/*********************************************************************
*   app_process.cpp
*
*   Copyright 2023 SkyAutoNet.
*
*   All Rights Reserved
**********************************************************************/
#include <app_process.h>


extern void open_Gst_Camera_Process(void);
extern void open_CleanUp_Process(void);

#ifdef USE_SVM
    extern void open_Svm_Ui_Process(void);
    extern void open_Calib_Ui_Process(void);
#endif

#ifdef USE_AICAM
    extern void open_AiCam_Ui_Process(void);
#endif


APP_PROCESS_ID backup_process_id;

void open_process_id(APP_PROCESS_ID screen_id)
{
    switch(screen_id)
    {
        case GST_CAMERA_PROCESS:
        {
            printf("\n************************GST_CAMERA_PROCESS************************\n");
            open_Gst_Camera_Process();
            break;
        }

        case CLEANUP_PROCESS:
        {
            printf("\n************************CLEANUP_PROCESS************************\n");
            open_CleanUp_Process();
            break;
        }

        #ifdef USE_SVM
            case SVM_UI_PROCESS:
            {
                printf("\n************************SVM_UI_PROCESS************************\n");
                open_Svm_Ui_Process();
                break;
            }

            case CALIB_UI_PROCESS:
            {
                printf("\n************************CALIB_UI_PROCESS************************\n");
                open_Calib_Ui_Process();
                break;
            }
        #endif

        #ifdef USE_AICAM
            case AICAM_UI_PROCESS:
            {
                printf("\n************************AICAM_UI_PROCESS************************\n");
                open_AiCam_Ui_Process();
                break;
            }
        #endif

        default:
        {
            printf("\n************************DEFAULT************************\n");

            #ifdef USE_SVM
                printf("\n************************SVM_UI_PROCESS************************\n");
                open_Svm_Ui_Process();
            #endif

            #ifdef USE_AICAM
                printf("\n************************AICAM_UI_PROCESS************************\n");
                open_AiCam_Ui_Process();
            #endif

            break;
        }
            
    }
}

