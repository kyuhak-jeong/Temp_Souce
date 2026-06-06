/*--------------------------------------------------------------------------*
 *                                                                          *
 *  FILE NAME : cleanup_process.cpp                                      	*
 *                                                                          *
 *  Copyright(c) 2025 by SkyAutoNet.                                        *
 *                                                                          *
 *--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*
  INCLUDE FILES
 *--------------------------------------------------------------------------*/
#include <cleanup_process.hpp>
#include "app_vars.h"


/*--------------------------------------------------------------------------*
  MACRO DEFINITIONS
 *--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*
  GLOBAL VARIABLE DECLARATIONS
 *--------------------------------------------------------------------------*/
CleanUpProcess  g_CleanUpProcess;

/*--------------------------------------------------------------------------*
  EXTERN VARIABLE DECLARATIONS
 *--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*
  EXTERN FUNCTION DECALRATIONS
 *--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*
  FUNCTION PROTOTYPES
 *--------------------------------------------------------------------------*/

void open_CleanUp_Process(void)
{
    g_CleanUpProcess.Create();
}


void* CleanUp_Thread( void * pArg )
{
	#if !defined(USE_SVM) && !defined(USE_AICAM)
		if(APP::app_ready == true)
        {
            APP::app_ready = false;
        }
		usleep(1000*1000);
	#endif

	APP::app_running = false;
	usleep(2000*1000);
	printf("\n----------taskFunction_CleanUp done-----------\n");
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
CleanUpProcess::CleanUpProcess()
{
}

CleanUpProcess::~CleanUpProcess()
{
}

void CleanUpProcess::Create(S32 _param1, S32 _param2, const char *_pTitle, SWinMain* _pParent)
{
	create_mainwin((CHAR*)_pTitle, _param1, _param2, _pParent);
}

void CleanUpProcess::Time_Callback(void)
{
	if(m_time_count >= 30)
	{
		if(system("sync")) {};
		m_time_count = 0;
	}
    m_time_count++;
}

void CleanUpProcess::Swm_CleanUp_Slickey(SMessage &_msg)
{
	switch (_msg.m_parameter1)
	{
		default:
			printf("cleanup key=%d\n",_msg.m_parameter1);
            send_message(SWM_CLOSE);
			break;
	}
}

void CleanUpProcess::pre_callback_procedure(SMessage &_msg)
{
	SMainWin::pre_callback_procedure(_msg);

	switch(_msg.m_message)
	{
		case SWM_INIT:

			m_CloseTick = NOTUSED;
			m_CloseWaitSec = 10; /* 200msec tick */
			if(0 != m_CloseWaitSec)
			{
				m_CloseTick = create_tick(this, m_CloseWaitSec);
				//printf("	- Call create_tick\n");
			}
			m_time_count = 0;
    
			tTaskCreate( &m_Task, CleanUp_Thread, &m_tAttrTask, (ArgTaskFn)NULL, 1 );

			break;

		case SWM_SYSPAINT:
			break;

		default:
			break;
	}
}

void CleanUpProcess::callback_procedure(SMessage &_msg)
{
	switch ( _msg.m_message )
	{
		case SWM_TICK:
			Time_Callback();
			break;

		case SWM_KEY:
			break;

		case SWM_SLICKEY:
			Swm_CleanUp_Slickey(_msg);
			break;

		default:
			break;
	}
}

void CleanUpProcess::post_callback_procedure(SMessage &_msg)
{
	SMainWin::post_callback_procedure(_msg);

	if(SWM_CLOSE == _msg.m_message)
	{
		if(m_CloseTick != NOTUSED)
		{
			destroy_tick(m_CloseTick);
			m_CloseWaitSec = 0;
		}

        backup_process_id = CLEANUP_PROCESS;

		if(system("sync")) {};

		if(PROCESS_ID_END > _msg.m_parameter1)
			open_process_id((APP_PROCESS_ID)_msg.m_parameter1);
	}
}
