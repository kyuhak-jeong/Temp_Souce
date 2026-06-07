/*********************************************************************
*
*   Copyright 2025 SkyAutoNet.
*
*   All Rights Reserved
*
*   No portion of this code may be copied or modified without the
*   prior written permission of SkyAutoNet.
*
**********************************************************************/
#include "app_process.h"
#include "app_vars.h"
#include "io_platform.h"
#include "ui_core.h"
#include "ui_elements.h"
#include "logger.h"

#ifdef USE_CAN
	#include "can_parser_main.h"
	#include "can_parser_sim.h"
	#include "can_parser_tatadaewoo.h"
	#include "can_parser_hyundai_eleccity.h"
	#include "can_parser_kia_bongo3ev.h"
	#include "can_parser_tatamotors_ebus.h"
	#include "can_parser_hyundai_new_county.h"

	#ifdef USE_MCU
		#include "aivmculib.h"
		using namespace AIVMCULib;
	#endif

	#ifdef USE_GPIO
		#include "san_gpio.h"
	#endif
#endif

using namespace APP;

#ifdef USE_SVM
	extern void SVM_handleTerminalCommand(std::string cmd);
	extern void SVM_handleInputEvent(const APP::IO::InputEvent& event);
	extern void CALIB_handleTerminalCommand(std::string cmd);
	extern void CALIB_handleInputEvent(const APP::IO::InputEvent& event);
#endif

#ifdef USE_AICAM
	extern void AI_handleTerminalCommand(std::string cmd);
	extern void AI_handleInputEvent(const IO::InputEvent& event);
#endif

#ifdef USE_DVR
	extern void stopDVR();

    extern void onCollisionDetected(float gForce, const std::string &direction, int v_level);
    extern void onManualTriggeredEvent();
    extern void onParkingMotionDetected();
	extern void DrSafePUAisActivated();

    extern void enterParkingMode();
    extern void exitParkingMode();

    extern void onFramerateChanged(int newFps);
    extern void onChannelCountChanged(int newChannelNum);
	extern void onAudioEnableChanged(bool newEnabled);
#endif

extern void gstCameraCleanup();

int bto_release_flag = 0;

///////////////////////////////////////////////////////////////////////////////
// Web Server Task 
///////////////////////////////////////////////////////////////////////////////

TaskT			m_Task_web;
TaskAttrT 		m_tAttrTask_web;

typedef void *	ArgTaskFn;

int fd_write;

const char *Goto_CleanUp = "go_clean";
const char *Goto_SvmUi = "go_svm";
const char *Goto_CalibUi = "go_calib";
const char *Goto_AiCamUi = "go_fl";

void Write_Webserver(const char* write_buffer)
{
	if(write(fd_write, write_buffer, sizeof(write_buffer))) {};
}

unsigned long Command_Function(char *command_value)
{
	unsigned long return_value;
	
	if(!strcmp(command_value, Goto_CleanUp))
	{
		return_value = (unsigned long)CLEANUP_PROCESS;		// CLEANUP_PROCESS = 1
	}
	else if(!strcmp(command_value, Goto_SvmUi))
	{
		return_value = (unsigned long)SVM_UI_PROCESS;		// SVM_UI_PROCESS = 3
	}
	else if(!strcmp(command_value, Goto_CalibUi))
	{
		return_value = (unsigned long)CALIB_UI_PROCESS;		// CALIB_UI_PROCESS = 4
	}
	else if(!strcmp(command_value, Goto_AiCamUi))
	{
		return_value = (unsigned long)AICAM_UI_PROCESS;	// AICAM_UI_PROCESS = 5
	}
	else
	{
		return_value = 0;
	}

	return return_value;
}

void * Webserver_Task( void * pArg )
{
	int fd_command;
    unsigned long    nKeyData = 0;

	const char * myfifo_command = "/tmp/pipe_command";
	mkfifo(myfifo_command, 0666);
	fd_command = open(myfifo_command, O_RDONLY);

	char read_command[80];
	while (APP::app_running)
	{
		memset(&read_command[0],0x0,sizeof(read_command));
		if(read(fd_command, &read_command[0], sizeof(read_command))) {};
		if(read_command[0]!=0)
		{
			printf("\n[WS] User: %s\n",&read_command[0]);
			nKeyData = Command_Function(&read_command[0]);
			if(nKeyData != 0)
			{
				// printf("SWAPI_SendKeyData %lu\n", nKeyData);
				SWAPI_SendKeyData( DEV_UPGRADE_KEY, (unsigned long)nKeyData );
				// open_process_id((APP_PROCESS_ID)nKeyData);
			}
		}
		usleep(100000);
	}
	close(fd_command);

	printf("Webserver Task Exit\n");

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
// Audio Task
///////////////////////////////////////////////////////////////////////////////

TaskT			m_Task_audio;
TaskAttrT 		m_tAttrTask_audio;

int audio_flag = 0;
int audio_sent = 0;


void * Audio_Task( void * pArg )
{
	#ifdef USE_BTO
		const double time_threshold = 3.0;
		bool         bto_activated  = false;
		double       last_activated = TimeUtils::nowSec();
	#endif

	while(APP::app_running)
	{
		#ifdef USE_SVM
			#ifdef USE_CAN
				if ((APP::svm_warning_flag[WARNING_FUNCTION_TYPE::W_MOBS][WARNING_ZONE_TYPE::W_RAISED] == 1) && (APP::vehicle_status.isStationary == false))
			#else
				if ((APP::svm_warning_flag[WARNING_FUNCTION_TYPE::W_MOBS][WARNING_ZONE_TYPE::W_RAISED] == 1))
			#endif
			{
				#ifdef DEVICE_RK3588
					if(system("aplay -D sysdefault:CARD=rockchiphdmi0 ../resources/audios/mois_bsis_alert.wav > /dev/null 2>&1 &")) {};
				#else
					if(system("aplay -D sysdefault:CARD=rockchiphdmi ../resources/audios/mois_bsis_alert.wav > /dev/null 2>&1 &")) {};
				#endif
			}

			#ifdef USE_CAN

				if (APP::svm_warning_flag[WARNING_FUNCTION_TYPE::W_LCA][WARNING_ZONE_TYPE::W_RAISED] == 1)
				{
					#ifdef DEVICE_RK3588
						if(system("aplay -D sysdefault:CARD=rockchiphdmi0 ../resources/audios/lca_alert.wav > /dev/null 2>&1 &")) {};
					#else
						if(system("aplay -D sysdefault:CARD=rockchiphdmi ../resources/audios/lca_alert.wav > /dev/null 2>&1 &")) {};
					#endif
				}

				#ifdef USE_BTO
					if (APP::svm_warning_flag[WARNING_FUNCTION_TYPE::W_BTO][WARNING_ZONE_TYPE::W_RAISED] == 1)
					{
						const OBDData obd = APP::g_obd.read();
						if ((obd.gearPos == GEAR::GEAR_DRIVING || obd.gearPos == GEAR::GEAR_REVERSE) && (APP::vehicle_status.isBTOActivate == true))
						{
							last_activated = TimeUtils::nowSec();

							if (bto_activated == false && APP::bto_status == 0)
							{
								write_uart_command("REQ,1;");
								bto_activated = true;
							}
						}
					}
					else // if CAN data is absent, treat as condition to deactivate BTO
					{
						if (bto_activated == true)
						{
							const OBDData obd = APP::g_obd.read();
							if (TimeUtils::nowSec() - last_activated >= time_threshold || obd.gearPos == GEAR::GEAR_PARKING || obd.gearPos == GEAR::GEAR_NEUTRAL)
							{
								write_uart_command("REQ,2;");
								bto_activated = false;
							}
						}
					}
				#endif // USE_BTO

			#endif // USE_CAN

		#endif // USE_SVM

		#ifdef USE_AICAM

			if (APP::aicam_warning_level[2] == 1 || APP::aicam_warning_level[2] == 2)
			{
                #if defined(DEVICE_RK3588)
				    if (system("aplay -D sysdefault:CARD=rockchiphdmi0 ../resources/audios/lca_alert.wav > /dev/null 2>&1")) {};
                #elif defined(DEVICE_RK3576)
				    if (system("aplay -D sysdefault:CARD=rockchiphdmi ../resources/audios/lca_alert.wav > /dev/null 2>&1")) {};
                #else
                   if (system("aplay ../resources/audios/lca_alert.wav > /dev/null 2>&1")) {}; 
                #endif
			}

			// if (APP::aicam_warning_level[1] == 1)
			// {
			// 	if (system("aplay -D sysdefault:CARD=rockchiphdmi ../resources/audios/alert_right.wav > /dev/null 2>&1")) {};
			// }

			// if (APP::aicam_warning_level[3] == 1)
			// {
			// 	if (system("aplay -D sysdefault:CARD=rockchiphdmi ../resources/audios/alert_left.wav > /dev/null 2>&1")) {};
			// }

			// usleep(100 * 1000);

		#endif

		usleep(100 * 1000);
	}

	#ifdef USE_BTO
		(void) time_threshold;
		(void) bto_activated;
		(void) last_activated;
	#endif

	printf("Audio Task Exit\n");
	return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// AICam GPIO Task (RK3576 Only)
///////////////////////////////////////////////////////////////////////////////

#if defined(USE_AICAM) && defined(DEVICE_RK3576)

	TaskT			m_Task_gpio;
	TaskAttrT 		m_tAttrTask_gpio;
	bool bto_active = false, gpio_active = false;

	void *Gpio_Task(void *pArg)
	{
		const double time_threshold = 3.0;
		double       gpio_last      = 0.0;
		#ifdef USE_BTO
			double   bto_last       = 0.0;
		#endif

		if (system("echo 120 > /sys/class/gpio/export")) {};

		usleep(100 * 1000);

		if (system("echo out > /sys/class/gpio/gpio120/direction")) {};
			
		printf("------------------Initialize GPIO done------------------\n");

		#ifdef USE_BTO
			write_uart_command("REQ,2;");
		#endif
		
		while (APP::app_running)
		{
			double current = TimeUtils::nowSec();

			#ifdef USE_BTO
				if ( (APP::aicam_warning_level[2] == 1) && bto_release_flag == 0)
				{
					if (bto_active == false)
					{
						write_uart_command("REQ,1;");
						if(APP::bto_status == 1) bto_active = true;
					}
					bto_last = TimeUtils::nowSec();
				}
				
				if ( bto_active == true && (APP::aicam_warning_level[2] == 2 || APP::aicam_warning_level[2] == 0) ) 
				{
					if (current - bto_last >= time_threshold)
					{
						write_uart_command("REQ,2;");
						if(APP::bto_status == 0) bto_active = false;
					}
				}
			#endif

			if ((APP::aicam_warning_level[1] == 1 || APP::aicam_warning_level[3] == 1) && gpio_active == false)
			{
				if (system("echo 1 > /sys/class/gpio/gpio120/value")) 
				{
					gpio_active = true;
					gpio_last   = TimeUtils::nowSec();
					printf("GPIO enabled for side cameras\n");
				}
			}
			
			if (gpio_active == true && APP::aicam_warning_level[1] == 0 && APP::aicam_warning_level[3] == 0) 
			{
				if (current - gpio_last >= time_threshold) 
				{
					if (system("echo 0 > /sys/class/gpio/gpio120/value")) 
					{
						gpio_active = false;
						printf("GPIO disabled\n");
					}
				}
			}

			usleep(10000); // 10ms 대기
		}

		if (system("echo 120 > /sys/class/gpio/unexport")) {};

		printf("GPIO Task Exit\n");

		return NULL;
	}

#endif

///////////////////////////////////////////////////////////////////////////////
// ACC Task
///////////////////////////////////////////////////////////////////////////////

// #if defined(USE_AICAM) && defined(DEVICE_RK3576)
#ifdef DEVICE_RK3576

	TaskT			m_Task_acc;
	TaskAttrT 		m_tAttrTask_acc;

	void * ACC_Check_Task( void * pArg )
	{
		const std::string gpio_path = "/sys/class/gpio/gpio23/value";
		
		if (system("echo 23 > /sys/class/gpio/export")) {};
		usleep(100 * 1000);

		while (APP::app_running == true)
		{
			std::ifstream gpio_file(gpio_path);
			std::string value;
			std::getline(gpio_file, value);

			gpio_file.close();

			if (value == "1") 
			{
				std::cout << "Acc Signal Disconnected, Cleanup and send Shutdown request" << std::endl;

				// APP::app_request_shutdown = true;
				int result = std::system("shutdown -h now");
				if (result != 0)
				{
					std::cerr << "Failed to shutdown" << std::endl;
				}
				else
				{
					std::cerr << "Bye" << std::endl;
				}

				Write_Webserver(Goto_CleanUp);

				break;
			} 
			else
			{
				// std::cout << "GPIO23 Value: " << value << std::endl;
			}

			usleep (1000 * 1000);
		}

		printf("ACC Task Exit\n");
		
		return NULL;
	}
	
#endif

///////////////////////////////////////////////////////////////////////////////
// Init OpenGL, GST, ImGUI
///////////////////////////////////////////////////////////////////////////////

bool InitializeOpenGL()
{
    LOG_APP_SECTION("Initializing Display with io_platform");
    
    APP::IO::Platform& platform = APP::IO::Platform::getInstance();
    
    // Initialize platform (handles EGL, display, GStreamer GL, input, etc.)
    if(platform.initialize("App UI", 1920, 1080) == false)
    {
        LOG_APP_ERROR("Failed to initialize io_platform");
        return false;
    }
    
    LOG_APP_SUCCESS("io_platform initialized successfully");
    
    // Get display size
    int width = 0, height = 0;
    platform.getDisplaySize(width, height, 0);
    APP::display_size[0] = width;
    APP::display_size[1] = height;
    
    LOG_APP_INFOF("Display size: %dx%d", width, height);
    
    // Make GL context current
    if (platform.makeGLContextCurrent() == false)
    {
        LOG_APP_ERROR("Failed to make GL context current");
        return false;
    }

	#ifdef USE_IMGUI
		// static APP::UI::IGCanvas mainIGCanvas;
		// if (mainIGCanvas.initialize(width, height) == true)
		// {
		// 	APP::main_canvas = &mainIGCanvas;
		// 	LOG_APP_SUCCESS("mainIGCanvas initialized");
		// }

		APP::UI::IGCanvas* mainIGCanvas = nullptr;
		mainIGCanvas = new APP::UI::IGCanvas();
		if (mainIGCanvas->initialize(width, height) == true)
		{
			APP::main_canvas = mainIGCanvas;
			LOG_APP_SUCCESS("mainIGCanvas initialized");
		}
	#endif

	#if defined(USE_GLES) && !defined(USE_IMGUI)
		// static APP::UI::GLCanvas mainGLCanvas;
		// if (mainGLCanvas.initialize(width, height) == true)
		// {
		// 	APP::main_canvas = &mainGLCanvas;
		// 	LOG_APP_SUCCESS("mainGLCanvas initialized");
		// }

		APP::UI::GLCanvas* mainGLCanvas = nullptr;
		mainGLCanvas = new APP::UI::GLCanvas();
		if (mainGLCanvas->initialize(width, height) == true)
		{
			APP::main_canvas = mainGLCanvas;
			LOG_APP_SUCCESS("mainGLCanvas initialized");
		}
	#endif

	// platform.releaseGLContext();

    return true;
}

void showLoadingScreen()
{
    if (APP::root_layout != nullptr && APP::main_view != nullptr)
    {
		APP::IO::Platform& platform = APP::IO::Platform::getInstance();
		std::string texName = "03_bg_loading_en";
		#if defined(USE_SVM) || defined(USE_AICAM)
        	texName = (APP::appConf.system_language == Language_English) ? "03_bg_loading_en" : "03_bg_loading_kr";
		#endif
        platform.beginFrame();
        APP::main_canvas->begin();
        APP::UI::Texture* loadingTex = APP::UI::TextureManager::getInstance().getTextureByName(texName);
        if (loadingTex != nullptr) APP::main_view->setTexture(loadingTex);
        APP::root_layout->onDraw(*APP::main_canvas);
        APP::main_canvas->end();
        platform.endFrame();
    }
}

void gstPipelineCleanup(GstData &gData, std::string name = "")
{
    gData.cleanup(true);
    printf(" gstPipeline %s Cleanup done\n", name.c_str());
}


void write_string_pipeline_config(std::string fileName)
{
	std::ofstream outfile(fileName);
	if (outfile.is_open()) 
	{
		#ifndef USE_CAMERA
			outfile << "video_path=" << APP::video_path << std::endl;
			outfile << "video_file_name=" << APP::video_file_name << std::endl;
		#endif

		outfile << "video_channels=" << APP::video_channels[0] << APP::video_channels[1] << APP::video_channels[2] << APP::video_channels[3] << std::endl;
	}
	outfile.close();
}

void read_string_pipeline_config(std::string fileName, std::string& video_path, std::string& video_file_name, int video_channels[4])
{
	std::ifstream file(fileName);

	if (file.is_open()) 
	{
		std::string line;
		while (std::getline(file, line)) 
		{
			std::istringstream iss(line);
			std::string key, value;
			if (std::getline(iss, key, '=') && std::getline(iss, value)) 
			{
				try 
				{
					#ifndef USE_CAMERA
						if (key == "video_path")
						{
							video_path = value;
						}
						else if (key == "video_file_name") 
						{
							video_file_name = value;
						}
					#endif

					
					if (key == "video_channels") 
					{
						int channels = std::stoi(value);
						for(int i = 0; i < 4; i++)
						{
							video_channels[i] = (channels / static_cast<int>(pow(10, 4 - i - 1))) % 10;
						}
					}
				} 
				catch (...) 
				{
					std::cout<< "Invalid argument" << std::endl;
				}
			}
		}
	}
	else
	{
		std::cout<< "No " << fileName << std::endl;
		write_string_pipeline_config(fileName);
		std::cout<< " => PipelineConfig: Written to file [" << fileName << "]" << std::endl;
	}

	file.close();
}

///////////////////////////////////////////////////////////////////////////////
// Backtrace Function
///////////////////////////////////////////////////////////////////////////////
static void signal_handler(int sig) 
{
    static bool bt_logged = false;
    void *array[20];
    size_t bt_size;
    bt_size = backtrace(array, 20);
    std::string dir = "sv_log/";
    std::string path = dir + "sv_backtrace_" + TimeUtils::getCurrentTimeString(true) + ".txt";
    std::ofstream sv_log(path);
    if (!sv_log)
    {
        printf("\nCan not open files: %s", path.c_str());
        
        if(!std::filesystem::is_directory(dir))
            mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        // exit(1);
    }
    else
    {
        sv_log << "Error: signal " << sig << ":\n";
        char **symbols = backtrace_symbols(array, bt_size);
        for (size_t i = 0; i < bt_size; i++) 
        {
            sv_log << symbols[i] << "\n";
        }
        free(symbols);
        if(bt_logged == false)
        {
            printf("\nSignal %d, Finished logging backtrace to %s\n", sig, path.c_str());
        }
    }
    sv_log.close();
    if(bt_logged == false)
    {
        Write_Webserver(Goto_CleanUp);
        bt_logged = true;
    }
}
static void register_signal(void) 
{
    signal( SIGTSTP, signal_handler );
    signal( SIGINT, signal_handler );
    signal( SIGABRT, signal_handler );
    signal( SIGSEGV, signal_handler );
    signal( SIGTERM, signal_handler );
}

///////////////////////////////////////////////////////////////////////////////
// GPS Task
///////////////////////////////////////////////////////////////////////////////

#if defined(USE_GPS) && defined(DEVICE_RK3588)

	TaskT			m_Task_gps;
	TaskAttrT 		m_tAttrTask_gps;
	
	void * GPS_Task( void * pArg )
	{
		GPS_Task();

        return nullptr;
	}

#endif  // GPS


///////////////////////////////////////////////////////////////////////////////
// IMU Task
///////////////////////////////////////////////////////////////////////////////

#if defined(USE_DVR) && defined(DEVICE_RK3588)

	TaskT			m_Task_imu;
	TaskAttrT 		m_tAttrTask_imu;
	
	void * IMU_Task( void * pArg )
	{
		imuData();

        return nullptr;
	}

#endif

///////////////////////////////////////////////////////////////////////////////
// BTO Task
///////////////////////////////////////////////////////////////////////////////

#if defined(USE_SERIAL) && defined(USE_DVR)

	#include <random>
	
	TaskT			m_Task_serial;
	TaskAttrT 		m_tAttrTask_serial;
	
	void * SerialCommunicationTask( void * pArg )
	{
		#if (1) // Timer for Event Recording
					auto s_last_pua_time = std::chrono::steady_clock::now();

			while (APP::app_running)
			{
				auto now     = std::chrono::steady_clock::now();
				auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - s_last_pua_time).count();

				if (elapsed >= 1)
				{
					s_last_pua_time = now;
					DrSafePUAisActivated();
				}

				std::this_thread::sleep_for(std::chrono::seconds(1));
			}

			return nullptr;
		#else
			std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
			std::uniform_int_distribution<int> dist(30, 60); // 30초 ~ 60초(1분)

			auto s_last_pua_time = std::chrono::steady_clock::now();
			int next_interval = dist(rng); // 첫 번째 호출 간격 설정

			while (APP::app_running)
			{
				auto now     = std::chrono::steady_clock::now();
				auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - s_last_pua_time).count();

				if (elapsed >= next_interval)
				{
					s_last_pua_time = now;
					next_interval   = dist(rng); // 다음 호출 간격 재설정
					DrSafePUAisActivated();
				}

				std::this_thread::sleep_for(std::chrono::seconds(1));
			}

			return nullptr;
		#endif

		// SerialDataReceiveTask();
		// return nullptr;
	}

#endif


///////////////////////////////////////////////////////////////////////////////
// Main
///////////////////////////////////////////////////////////////////////////////
void handleTerminalCommandCallback(std::string cmd)
{
    // printf("Parsing command [%s].\n", cmd.c_str());

    auto d = cmd.c_str();

    // Process Web Server commands first
    if(d[0] == 'W' && d[1] == 'S' && d[2] == ' ' && d[3] == '1')
    {
        Write_Webserver(Goto_CleanUp);  // CLEANUP_PROCESS = 1
    }
    #ifdef USE_SVM
        else if(d[0] == 'W' && d[1] == 'S' && d[2] == ' ' && d[3] == '3')
        {
            Write_Webserver(Goto_SvmUi);    // SVM_UI_PROCESS = 3
        }
        else if(d[0] == 'W' && d[1] == 'S' && d[2] == ' ' && d[3] == '4')
        {
            Write_Webserver(Goto_CalibUi);  // CALIB_UI_PROCESS = 4
        }
    #endif
    #ifdef USE_AICAM
        else if(d[0] == 'W' && d[1] == 'S' && d[2] == ' ' && d[3] == '5')
        {
            Write_Webserver(Goto_AiCamUi);  // AICAM_UI_PROCESS = 5
        }
    #endif

    #ifdef USE_DVR
		if(d[0] == 'R')
		{
			if(d[1] == 'E' && d[2] == ' ')
			{
				if(d[3] == 'C')
				{
					if(d[4] == '1')         onCollisionDetected(1.0f, "North", 0);
					else if(d[4] == '2')    onCollisionDetected(2.0f, "East", 0);
					else if(d[4] == '3')    onCollisionDetected(2.0f, "West", 0);
					else if(d[4] == '4')    onCollisionDetected(2.0f, "South", 0);
				}
				else if(d[3] == 'M')
				{
					onManualTriggeredEvent();
				}
				else if(d[3] == 'P')
				{
					onParkingMotionDetected();
				}
				else if(d[3] == 'A')
				{
					DrSafePUAisActivated();
				}
			}
			else if(d[1] == 'P' && d[2] == ' ')
			{
				if(d[3] == '0')         exitParkingMode();
				else if(d[3] == '1')    enterParkingMode();
			}
			else if(d[1] == 'F' && d[2] == ' ')
			{
				if(d[3] == '0')         onFramerateChanged(5);
				else if(d[3] == '1')    onFramerateChanged(10);
				else if(d[3] == '2')    onFramerateChanged(20);
				else if(d[3] == '3')    onFramerateChanged(30);
			}
			else if(d[1] == 'C' && d[2] == ' ')
			{
				onChannelCountChanged((int)(d[3] - '0'));
			}
			else if(d[1] == 'A' && d[2] == ' ')
			{
				onAudioEnableChanged((bool)(d[3] - '0'));
			}

			return;
		}
    #endif


    // Common commands for SVM and AICAM
	#ifdef USE_SVM
		SVM_handleTerminalCommand(cmd);
		CALIB_handleTerminalCommand(cmd);
	#endif

	#ifdef USE_AICAM
		AI_handleTerminalCommand(cmd);
	#endif

}

void handleInputCallback(const IO::InputEvent& event)
{
	#ifdef USE_SVM
		SVM_handleInputEvent(event);
		CALIB_handleInputEvent(event);
	#endif

	#ifdef USE_AICAM
		AI_handleInputEvent(event);
	#endif
}

static int remoteDeviceCount = 0;
void handleDeviceConnectionCallback(const std::string &deviceName, int eventNumber, IO::DeviceType type, bool connected)
{
	if(deviceName.find("HID") != std::string::npos || deviceName.find("Remote") != std::string::npos)
	{
		if (connected)
		{
			remoteDeviceCount++;
			APP::ble_connect = true;
		}
		else
		{
			remoteDeviceCount--;
			if (remoteDeviceCount <= 0)
			{
				remoteDeviceCount = 0;
				APP::ble_connect = false;
			}
		}
	}
}

void setupMainScreenUI()
{
    // // Load fonts
    // APP::UI::FontManager::getInstance().initialize();
	// APP::UI::FontManager::getInstance().loadFont(APP::UI::FontConfig("default", "NotoSansKR-Bold",
	// 													   std::string(_FONTS_PATH_) + "/NotoSansKR-Bold.ttf",
	// 													   60.0f, APP::UI::CharacterSet::KOREAN, 2048, 2048));
	// APP::UI::FontManager::getInstance().setDefaultFont("default");
    
    // Load textures
	APP::UI::TextureManager::getInstance().loadTextureCollection(std::string(_TEXTURES_PATH_) + "/ui", "ui_list.txt");
    
    // Create root layout
    APP::root_layout = new APP::UI::FrameLayout();
    APP::root_layout->getLayoutParams().width = APP::UI::MATCH_PARENT;
    APP::root_layout->getLayoutParams().height = APP::UI::MATCH_PARENT;
    
    APP::main_view = new APP::UI::ImageView();
    APP::main_view->getLayoutParams().width = APP::UI::MATCH_PARENT;
    APP::main_view->getLayoutParams().height = APP::UI::MATCH_PARENT;
    APP::main_view->setScaleType(APP::UI::ScaleType::FIT_XY);
	APP::main_view->setCornerRadius(50.0f);
    APP::root_layout->addView(APP::main_view);
    
    // Measure and layout
    APP::UI::MeasureSpec widthSpec = APP::UI::MeasureSpec::makeExactly(static_cast<float>(APP::display_size[0]));
    APP::UI::MeasureSpec heightSpec = APP::UI::MeasureSpec::makeExactly(static_cast<float>(APP::display_size[1]));
    APP::root_layout->onMeasure(widthSpec, heightSpec);
    APP::root_layout->onLayout(APP::UI::RectF::fromXYWH(0, 0, APP::display_size[0], APP::display_size[1]));
}

int main(int argc, char* argv[])
{
	register_signal();
		
	APP::app_running = true;
	APP::app_request_shutdown = false;

	#if defined(USE_SVM) || defined(USE_AICAM)
		APP::app_ready = false;
	#else
		APP::app_ready = true;
	#endif
	
	if(system("ipcrm --all;")) {};

	if(system("clear;")) {};

	std::cout << "\n========================================APP START=================================================" << std::endl;

	read_string_pipeline_config(pipeline_conf_file, APP::video_path, APP::video_file_name, APP::video_channels);

	#ifndef USE_CAMERA
		std::cout << "Use video!!! video_path: " << APP::video_path << ", video_file_name: " << APP::video_file_name 
		<< (std::filesystem::exists(APP::video_path + APP::video_file_name) ? "[existed]" : "[not existed]")
		<< ", video_channels: " << APP::video_channels[0] << APP::video_channels[1] << APP::video_channels[2] << APP::video_channels[3] << std::endl;
	#else 
		std::cout << "Use camera!!! video_channels: " << APP::video_channels[0] << APP::video_channels[1] << APP::video_channels[2] << APP::video_channels[3] << std::endl;
	#endif

	// Load local internal configuration for DVR
	#ifdef USE_DVR
		APP::dvrConf.loadConfig();
	#endif

	// Load local configuration for UI App
	#if defined(USE_SVM) || defined(USE_AICAM)
		#ifdef USE_AICAM
			APP::appConf.initAiCamConfig();
            #ifdef USE_TAXI
    			APP::appConf.initTaxiConfig();
            #endif
		#endif

		if(APP::appConf.readConfig(app_conf_file.c_str()) == false)
		{
			#ifdef USE_DVR
				APP::appConf.initDvrConfig();
			#endif

			APP::appConf.writeConfig(app_conf_file.c_str(), true);
		}

		// Set Internal DVR config that link with UI Config
		#ifdef USE_DVR
			if(APP::appConf.syncDvrConfig(APP::dvrConf) == true)
			{
				APP::dvrConf.updateStoragePath();
				APP::dvrConf.writeConfig();
			}
		#endif
	#endif

	#ifdef USE_DVR
		APP::storage_path      = APP::dvrConf.storageDevPath;
		APP::storage_connected = (APP::storage_path != "/");
		APP::dvrConf.printConfig();
	#endif

	usleep(50*1000);


	SWAPI_InitSWindow(NULL);

	tTaskCreate( &m_Task_audio, Audio_Task, &m_tAttrTask_audio, (ArgTaskFn)NULL, 1 );

	tTaskCreate( &m_Task_web, Webserver_Task, &m_tAttrTask_web, (ArgTaskFn)NULL, 1 );

	#if defined(DEVICE_RK3588) && defined(USE_GPS) && defined(USE_DVR)
		tTaskCreate( &m_Task_gps, GPS_Task, &m_tAttrTask_gps, (ArgTaskFn)NULL, 1 );
	#endif

	#if defined(DEVICE_RK3588) && defined(USE_DVR)
		tTaskCreate( &m_Task_imu, IMU_Task, &m_tAttrTask_imu, (ArgTaskFn)NULL, 1 );
	#endif

	#if defined(USE_SERIAL) && defined(USE_DVR)
		tTaskCreate( &m_Task_serial, SerialCommunicationTask, &m_tAttrTask_serial, (ArgTaskFn)NULL, 1 );
	#endif

	#if defined(USE_CAN) && !defined(USE_TAXI)
		#ifdef DEVICE_RK3588
			tTaskCreate( &m_Task_canrx, Canrx_RK3588_Task, &m_tAttrTask_canrx, (ArgTaskFn)NULL, 1 );
		#endif
		#ifdef DEVICE_RK3576
			tTaskCreate( &m_Task_canrx, CanRx_RK3576_Task, &m_tAttrTask_canrx, (ArgTaskFn)NULL, 1 );
		#endif
	#endif

	#if defined(USE_AICAM) && defined(DEVICE_RK3576)
		tTaskCreate( &m_Task_gpio, Gpio_Task, &m_tAttrTask_gpio, (ArgTaskFn)NULL, 1 );
	#endif

	#ifdef DEVICE_RK3576
		tTaskCreate( &m_Task_acc, ACC_Check_Task, &m_tAttrTask_acc, (ArgTaskFn)NULL, 1 );
	#endif

	#ifdef USE_TAXI
		tTaskCreate( &m_Task_gpio_taxi, GpioTaxi_Task, &m_tAttrTask_gpio_taxi, (ArgTaskFn)NULL, 1 );
	#endif

    
    const char * myfifo = "/tmp/pipe_command";
    mkfifo(myfifo, 0666);
    fd_write = open(myfifo, O_WRONLY);
    

	// Initialize OpenGL/Display using io_platform
    if(InitializeOpenGL() == false)
    {
        LOG_APP_ERROR("Failed to initialize OpenGL");
        return -1;
    }
    
    // Get platform instance for input handling
    APP::IO::Platform& platform = APP::IO::Platform::getInstance();
	platform.setInputCallback(handleInputCallback);
	platform.setTerminalCommandCallback(handleTerminalCommandCallback);
	platform.setDeviceConnectionCallback(handleDeviceConnectionCallback);
	
    // Initialize main screen
	setupMainScreenUI();

	if(APP::main_canvas != nullptr && APP::root_layout != nullptr && APP::main_view != nullptr)
	{
		platform.beginFrame();
		APP::main_canvas->begin();
		APP::main_view->setTexture(APP::UI::TextureManager::getInstance().getTextureByName("01_intro_safeview_plus"));
		APP::root_layout->onDraw(*APP::main_canvas);
		APP::main_canvas->end();
		platform.endFrame();
	}

	APP::IO::Platform::getInstance().releaseGLContext();
    
    // Open menu screens
    open_process_id(GST_CAMERA_PROCESS);
    usleep(100*1000);
    
    #ifdef USE_SVM
        open_process_id(SVM_UI_PROCESS);
    #endif
    
    #ifdef USE_AICAM
        open_process_id(AICAM_UI_PROCESS);
    #endif
    
    LOG_APP_SECTION("Entering main loop");
    
    // Main loop
    while(APP::app_running == true && platform.shouldClose() == false)
    {
        platform.pollInput();
        platform.updateDisplayPower();
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // 60 FPS
    }
    
    LOG_APP_SECTION("Exiting main loop - Starting cleanup");
    
    // Cleanup resources
	platform.makeGLContextCurrent();
    
	if(APP::root_layout != nullptr)
    {
		// delete root_layout only, child will be auto deleteds
        delete APP::root_layout;
        APP::root_layout = nullptr;
		APP::main_view = nullptr;
    }

	if(APP::main_canvas != nullptr)
	{
		APP::main_canvas->shutdown();
		delete APP::main_canvas;
		APP::main_canvas = nullptr;
	}

	// Clean up textures & fonts
    APP::UI::TextureManager::getInstance().clearAll();
    // APP::UI::FontManager::getInstance().cleanup();

	close(fd_write);
	if(system("rm -rf /tmp/pipe_command")) {};
	if(system("sync")) {};

	usleep(2500*1000);

	platform.releaseGLContext();
	platform.shutdown();
	
    LOG_APP_SUCCESS("Cleanup complete");
    LOG_APP_SECTION("Application Closed");
    
    return 0;
}
