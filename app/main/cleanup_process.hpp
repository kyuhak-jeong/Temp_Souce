/*--------------------------------------------------------------------------*
 *                                                                          *
 *  FILE NAME : cleanup_process.hpp                                         *
 *                                                                          *
 *  Copyright(c) 2025 by SkyAutoNet.                                        *
 *                                                                          *
 *--------------------------------------------------------------------------*/
#ifndef	__CLEANUP_PROCESS_H__
#define	__CLEANUP_PROCESS_H__

/*--------------------------------------------------------------------------*
  INCLUDE FILES
 *--------------------------------------------------------------------------*/
#include <app_process.h>

/*--------------------------------------------------------------------------*
  MACRO DEFINITIONS
 *--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*
  GLOBAL VARIABLE DECLARATIONS
 *--------------------------------------------------------------------------*/
class CleanUpProcess : public SMainWin
{
private:

	int 	      m_CloseWaitSec;
	tick_t	    m_CloseTick;

	TaskT		    m_Task;
	TaskAttrT		m_tAttrTask;

  int         m_time_count = 0;

public:
    CleanUpProcess();
    ~CleanUpProcess();

    void Create (
                    S32 _param1 = 0,
                    S32 _param2 = 0,
                    const char *_pTitle = "CleanUpProcess" ,
                    SWinMain* _pParent = NULL );
	
	  void Swm_CleanUp_Slickey(SMessage &_msg);

	  void Time_Callback(void);

    void pre_callback_procedure(SMessage &_msg);

    void callback_procedure(SMessage &_msg);

    void post_callback_procedure(SMessage &_msg);
};

#endif  // __CLEANUP_PROCESS_H__

