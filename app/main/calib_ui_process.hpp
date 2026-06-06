/*--------------------------------------------------------------------------*
 *                                                                          *
 *  FILE NAME : calib_ui_process.hpp                                        *
 *                                                                          *
 *  Copyright(c) 2025 by SkyAutoNet.                                        *
 *                                                                          *
 *--------------------------------------------------------------------------*/

#ifndef CALIB_UI_PROCESS_H
#define CALIB_UI_PROCESS_H

#include "app_process.h"

// ============================================================================
// Process Entry Points
// ============================================================================

void open_Calib_Ui_Process(void);
void CALIB_handleTerminalCommand(std::string cmd);
void CALIB_handleInputEvent(const APP::IO::InputEvent& event);

// ============================================================================
// CalibUiProcess Class
// ============================================================================

class CalibUiProcess : public SMainWin
{
public:
    CalibUiProcess();
    ~CalibUiProcess();

    void Create(S32 _param1 = 0, S32 _param2 = 0, const char* _pTitle = "CalibUiProcess", SWinMain* _pParent = nullptr);
    void Time_Callback();
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

#endif  // CALIB_UI_PROCESS_H