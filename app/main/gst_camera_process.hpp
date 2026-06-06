/*--------------------------------------------------------------------------*
 *                                                                          *
 *  FILE NAME : gst_camera_process.hpp                                      *
 *                                                                          *
 *  Copyright(c) 2025 by SkyAutoNet.                                        *
 *                                                                          *
 *--------------------------------------------------------------------------*/
#ifndef	GST_CAMERA_PROCESS_H
#define	GST_CAMERA_PROCESS_H

#include "app_process.h"

// ============================================================================
// Process Entry Points
// ============================================================================

void  open_Gst_Camera_Process(void);

// ============================================================================
// GstCameraProcess Class
// ============================================================================

class GstCameraProcess : public SMainWin
{
public:
    GstCameraProcess();
    ~GstCameraProcess();
    
    void Create(S32 _param1 = 0, S32 _param2 = 0, const char* _pTitle = "GstCameraProcess", SWinMain* _pParent = nullptr);
    void Time_Callback(void);
    void Swm_Slickey(SMessage& _msg);
    
protected:
    void pre_callback_procedure(SMessage& _msg)  override;
    void callback_procedure(SMessage& _msg)      override;
    void post_callback_procedure(SMessage& _msg) override;
    
private:
    TaskT     m_Task;
    TaskAttrT m_tAttrTask;
    S32       m_CloseTick    = NOTUSED;
    S32       m_CloseWaitSec = 10;
    U32       m_timeCount    = 0;
};

#endif  // GST_CAMERA_PROCESS_H

