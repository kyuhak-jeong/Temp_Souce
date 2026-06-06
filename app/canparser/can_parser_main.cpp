#include "can_parser_main.h"
#include "app_vars.h"
#include "app_process.h"

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

// ============================================================
// Shared State
// ============================================================

int64_t left_on_timestamp  = 0;
int64_t right_on_timestamp = 0;

std::atomic<GEAR> globalGearState = GEAR::GEAR_NEUTRAL;

// ============================================================
// Signal / Gear Helpers
// ============================================================

int CalculateTurnSignalInterval(int left, int right)
{
    int64_t now = TimeUtils::nowMs();

    if (left)  left_on_timestamp  = now;
    if (right) right_on_timestamp = now;

    bool left_on  = (now - left_on_timestamp  <= 600);
    bool right_on = (now - right_on_timestamp <= 600);

    if (left == 0 && right == 0 && left_on == false && right_on == false) return 0;
    if (left_on == true && right_on == true) return 3;
    if (right_on == true)                    return 2;
    if (left_on == true)                     return 1;

    return 0;
}

GEAR mapGearToStandard(int gear, int vehicle_flag)
{
    switch (vehicle_flag)
    {
        case 1:
            if (gear == 1)   return GEAR::GEAR_DRIVING;
            if (gear == 126) return GEAR::GEAR_REVERSE;
            break;
        case 2:
        case 3:
            if (gear >= 3)  return GEAR::GEAR_DRIVING;
            if (gear <= -1) return GEAR::GEAR_REVERSE;
            if (gear == 0)  return GEAR::GEAR_NEUTRAL;
            break;
        case 4:
        case 5:
        case 6:
            if (gear == 1)   return GEAR::GEAR_DRIVING;
            if (gear == 0)   return GEAR::GEAR_NEUTRAL;
            if (gear == -1)  return GEAR::GEAR_REVERSE;
            if (gear == 126) return GEAR::GEAR_PARKING;
            break;
        default:
            return GEAR::GEAR_NEUTRAL;
    }
    return GEAR::GEAR_NEUTRAL;
}

std::array<bool, 4> SelectedCamIndexforInference(const OBDData& obd)
{
    std::array<bool, 4> enabled = {false, false, true, false}; // Front, Right, Rear, Left

    if(obd.turnSignal == TURN_SIGNAL::TURN_SIGNAL_EMERGENCY)
    {
        enabled = {false, false, true, false};
    }

    // if (obd.gearPos == GEAR::GEAR_REVERSE)
    // {
    //     enabled = {false, false, true, false};
    // }
    // else if (obd.gearPos == GEAR::GEAR_DRIVING && obd.turnSignal == TURN_SIGNAL::TURN_SIGNAL_LEFT)
    // {
    //     enabled = {true, false, false, true};
    // }
    // else if (obd.gearPos == GEAR::GEAR_DRIVING && obd.turnSignal == TURN_SIGNAL::TURN_SIGNAL_RIGHT)
    // {
    //     enabled = {true, true, false, false};
    // }
    // else if (obd.gearPos == GEAR::GEAR_DRIVING && obd.turnSignal == TURN_SIGNAL::TURN_SIGNAL_EMERGENCY)
    // {
    //     enabled = {true, true, true, true};
    // }
    else
    {
        enabled = {true, false, false, false};
    }

    return enabled;
}

// ============================================================
// Vehicle Data Processing
// ============================================================

void Process_Vehicle_Speed_Status(int vehicle_speed, int gear_pos)
{
    if (vehicle_speed >= -1 && vehicle_speed <= 1)
    {
        APP::vehicle_status.isStationary  = true;
        APP::vehicle_status.isBTOActivate = true;
    }
    else if (APP::vehicle_status.isBTOActivate == true && vehicle_speed >= 5)
    {
        APP::vehicle_status.isBTOActivate = false;
    }
    else
    {
        APP::vehicle_status.isStationary = false;
    }
}

void Process_Vehicle_Canrx_Info(const AIVMCULib::VehicleInfo_t& veh_inf)
{
    const GEAR   gear   = mapGearToStandard(veh_inf.gear, APP::vehicle_model);
    const int    signal = CalculateTurnSignalInterval(veh_inf.turn_signal & 0x1, (veh_inf.turn_signal >> 1) & 0x1);
    const int    angle  = static_cast<int>(veh_inf.str_whl_rad * 180.0f / M_PI);

    DataUtils::obd_update(APP::g_obd, veh_inf.speed_kmh, veh_inf.rpm, gear, signal, angle);

    globalGearState.store(gear, std::memory_order_relaxed);

    const OBDData obd = APP::g_obd.read();
    Process_Vehicle_Speed_Status(obd.speedKmh, obd.gearPos);
    APP::InputSelector = SelectedCamIndexforInference(obd);
}

void Process_Vehicle_Canrx_Info(const AIVMCULib::VehicleInfo_t& veh_inf, const AIVMCULib::McuStatus_t& mcu_status)
{
    GEAR gear   = mapGearToStandard(veh_inf.gear, APP::vehicle_model);
    int  signal = CalculateTurnSignalInterval(veh_inf.turn_signal & 0x1, (veh_inf.turn_signal >> 1) & 0x1);
    int  angle  = static_cast<int>(veh_inf.str_whl_rad * 180.0f / M_PI);

    #if (defined(DEVICE_RK3588) || defined(DEVICE_RK3576)) && defined(USE_GPIO) && !defined(USE_TAXI)
        gear   = (mcu_status.gpio[0] != 0) ? GEAR::GEAR_REVERSE : GEAR::GEAR_DRIVING;
        signal = CalculateTurnSignalInterval(mcu_status.gpio[1], mcu_status.gpio[2]);
    #endif

    #if defined(DEVICE_RK3576) && defined(USE_GPIO) && defined(USE_TAXI)
        gear   = (mcu_status.gpio[0] != 0) ? GEAR::GEAR_REVERSE : GEAR::GEAR_DRIVING;
        signal = CalculateTurnSignalInterval(mcu_status.gpio[1], mcu_status.gpio[2]);
    #endif

    #ifndef USE_TAXI
        DataUtils::obd_update(APP::g_obd, veh_inf.speed_kmh, veh_inf.rpm, gear, signal, angle);
    #else  
        DataUtils::obd_set_gear(APP::g_obd, static_cast<int32_t>(gear));
        DataUtils::obd_set_signal(APP::g_obd, signal);
    #endif

    const OBDData obd = APP::g_obd.read();
    Process_Vehicle_Speed_Status(obd.speedKmh, obd.gearPos);
    APP::InputSelector = SelectedCamIndexforInference(obd);

    (void) angle;
}

// ============================================================
// RK3588 CAN Task
// ============================================================

#ifdef DEVICE_RK3588

    TaskT     m_Task_canrx;
    TaskAttrT m_tAttrTask_canrx;

    std::shared_ptr<AIVMCULib::McuMgr> 	g_pMcu_mgr;
    std::unique_ptr<AIVCANParser> can_parser = nullptr;
    bool g_mcu_available = true;

    void* Canrx_RK3588_Task(void* pArg)
    {
        AIVMCULib::McuStatus_t				mcu_status;
        AIVMCULib::VehicleInfo_t			veh_inf = {};
        std::unique_ptr<AIVCANParser> can_parser = nullptr;

        switch(APP::vehicle_model)
        {
            case VEHICLE_MODEL::TataDaewoo:
                can_parser = std::make_unique<AIVCANParserTataDaewoo>("can0");
                std::cout << "Selected Vehicle: TATA Daewoo" << std::endl;
                break;
            case VEHICLE_MODEL::HyundaiElecCity:
                can_parser = std::make_unique<AIVCANParserHyundaiElecCity>("can0");
                std::cout << "Selected Vehicle: HyundaiEleCity" << std::endl;
                break;
            case VEHICLE_MODEL::KiaBongo3Ev:
                can_parser = std::make_unique<AIVCANParserKiaBongo3Ev>("can0");
                std::cout << "Selected Vehicle: KiaBongo3Ev" << std::endl;
                break;
            case VEHICLE_MODEL::TataMotorsEbus:
                can_parser = std::make_unique<AIVCANParserTataMotorsEbus>("can0");
                std::cout << "Selected Vehicle: TataMotorsEbus" << std::endl;
                break;
            case VEHICLE_MODEL::HyundaiNewCounty:
                can_parser = std::make_unique<AIVCANParserHyundaiNewCounty>("can0");
                std::cout << "Selected Vehicle: HyundaiNewCounty" << std::endl;
                break;
            case VEHICLE_MODEL::Simulator:
            default:
                can_parser = std::make_unique<AIVCANParserSim>("can0");
                std::cout << "Selected Vehicle: Seda-JigSimulator" << std::endl;
                break;
        }

        g_pMcu_mgr = AIVMCULib::McuMgr::create();

        while(APP::app_running && g_mcu_available == true)
        {
            int ret = g_pMcu_mgr->GetMcuStatus(mcu_status);

            if(ret != 0)
            {
                if(ret == -ENODATA)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    continue;
                }
                printf("GetMcuStatus failed %s\r\n", AIVMCULib::getErrorDesc(ret));
                g_mcu_available = false;
            }
            break;
        }

        if(can_parser != nullptr) can_parser->start();

        printf("\n------------------CanRX Receiving Started------------------\n");

        while(APP::app_running)
        {
            if(g_mcu_available == false)
            {
                printf("CanRx Task: Failed to get MCU status => Exit\n");
                break;
            }

            int mcu_ret = g_pMcu_mgr->GetMcuStatus(mcu_status); // if you want to parsing gpio data then enable;
            int ret = can_parser->getVehicleInfo(veh_inf);

            if (ret == 0 && mcu_ret == 0) // Possible to Get Vehicle Info from AIVMCULib Parser.
            {
                Process_Vehicle_Canrx_Info(veh_inf);
            }
            else
            {
                printf("failed to get mcu vehicle info\n");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100000));

        }

        if(can_parser != nullptr)
        {
            can_parser->stop();
            // delete can_parser;
        }

        printf("CanRx Task Exit\n");
        return NULL;
    }
#endif

// ============================================================
// RK3576 CAN Task
// ============================================================

#if defined(DEVICE_RK3576) && !defined (USE_TAXI)

    TaskT     m_Task_canrx;
    TaskAttrT m_tAttrTask_canrx;

    void* CanRx_RK3576_Task(void* pArg)
    {
        struct sockaddr_can addr;
        struct ifreq ifr;

        AIVMCULib::VehicleInfo_t veh_inf = {};

        std::unique_ptr<AIVCANParser> can_parser = nullptr;
        
        switch(APP::vehicle_model)
        {
            case VEHICLE_MODEL::TataDaewoo:
                can_parser = std::make_unique<AIVCANParserTataDaewoo>("can0");
                std::cout << "Selected Vehicle: TATA Daewoo" << std::endl;
                break;
            case VEHICLE_MODEL::HyundaiElecCity:
                can_parser = std::make_unique<AIVCANParserHyundaiElecCity>("can0");
                std::cout << "Selected Vehicle: HyundaiEleCity" << std::endl;
                break;
            case VEHICLE_MODEL::KiaBongo3Ev:
                can_parser = std::make_unique<AIVCANParserKiaBongo3Ev>("can0");
                std::cout << "Selected Vehicle: KiaBongo3Ev" << std::endl;
                break;
            case VEHICLE_MODEL::TataMotorsEbus:
                can_parser = std::make_unique<AIVCANParserTataMotorsEbus>("can0");
                std::cout << "Selected Vehicle: TataMotorsEbus" << std::endl;
                break;
            case VEHICLE_MODEL::HyundaiNewCounty:
                can_parser = std::make_unique<AIVCANParserHyundaiNewCounty>("can0");
                std::cout << "Selected Vehicle: HyundaiNewCounty" << std::endl;
                break;
            case VEHICLE_MODEL::Simulator:
            default:
                can_parser = std::make_unique<AIVCANParserSim>("can0");
                std::cout << "Selected Vehicle: Seda-JigSimulator" << std::endl;
                break;
        }

        if(system("ip link set can0 down")) {};
        if(system("ip link set can0 type can bitrate 500000 dbitrate 2000000 fd on")) {};
        if(system("ip link set can0 up")) {};

        int can_socket;

        can_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW);

        if (can_socket < 0)
        {
            perror("socket");
            exit(1);
        }

        strcpy(ifr.ifr_name, "can0");
        if (ioctl(can_socket, SIOCGIFINDEX, &ifr) < 0) 
        {
            perror("ioctl");
            exit(1);
        }

        addr.can_family = AF_CAN;
        addr.can_ifindex = ifr.ifr_ifindex;

        if (bind(can_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) 
        {
            perror("bind");
            exit(1);
        }
        
        int32_t hPoll = 0;
        struct pollfd   pollEvent;
        pollEvent.fd = can_socket;
        pollEvent.revents = 0;
        pollEvent.events = POLLIN;
        
        printf("\n------------------CanRX Receiving Started------------------\n");

        while(APP::app_running)
        {
            hPoll = poll( (struct pollfd*)&pollEvent, 1, 1000 ); // 1000ms Timeout
            {
                if (hPoll < 0)
                {
                    perror("poll error\r\n");
                    continue;
                }
                else if (hPoll == 0)
                {
                    // printf("poll timeout\r\n");
                    continue;
                }
            }

            struct can_frame frame;
            int nbytes = read(can_socket, &frame, sizeof(struct can_frame));

            #ifdef USE_GPIO
                    for (int i=0; i < GPIO_PINS_NUM; i++) // gpio 1~4 Line
                    {
                        mcu_status.gpio[i] = SAN_GpioGetValue(hGpio[i]);
                        // printf("mcu_status.gpio[%d]: %d\n", i, SAN_GpioGetValue(hGpio[i]));
                    }
            #endif

            if(nbytes > 0)
            {
                if(can_parser->parse(frame, veh_inf))
                {
                    #ifdef USE_GPIO
                        Process_Vehicle_Canrx_Info(veh_inf, mcu_status);
                    #else
                        Process_Vehicle_Canrx_Info(veh_inf);
                    #endif
                }
            }
            else if (nbytes < 0)
            {
                perror("CAN read error\r\n");
            }

            // std::this_thread::sleep_for(std::chrono::microseconds(100000));
        }

        #ifdef USE_GPIO
            for (int i = 0; i < GPIO_PINS_NUM; i++)
            {
                if (hGpio[i] != nullptr)
                {
                    SAN_GpioDeinit(hGpio[i]);
                }
            }
        #endif

        if(can_parser != nullptr) can_parser->stop();

        printf("CanRx Task Exit\n");
        close(can_socket);

        return NULL;
    }
#endif

// ============================================================
// RK3576 GPIO Task for Taxi
// ============================================================

#ifdef USE_TAXI

    TaskT     m_Task_gpio_taxi;
    TaskAttrT m_tAttrTask_gpio_taxi;

    void* GpioTaxi_Task(void* pArg)
    {
        AIVMCULib::McuStatus_t mcu_status = {};

        SAN_GPIO_HANDLE hGpio[GPIO_PINS_NUM] = {nullptr};
        int gpio_pins[GPIO_PINS_NUM] = {108, 109, 110, 111};

        for (int i = 0; i < GPIO_PINS_NUM; i++)
        {
            hGpio[i] = SAN_GpioInit(gpio_pins[i]);
            SAN_GpioDirection(hGpio[i], GPIO_DIRECTION_IN);
        }

        printf("\n------------------GPIO Taxi Task Started------------------\n");

        while (APP::app_running)
        {
            for (int i = 0; i < GPIO_PINS_NUM; i++)
            {
                mcu_status.gpio[i] = SAN_GpioGetValue(hGpio[i]);
                // printf("gpio[%d]: %d\t", i, mcu_status.gpio[i]);
            }
            // printf("\r\n");
            Process_Vehicle_Canrx_Info({}, mcu_status);
            usleep(10 * 1000);
        }

        for (int i = 0; i < GPIO_PINS_NUM; i++)
        {
            if (hGpio[i] != nullptr) SAN_GpioDeinit(hGpio[i]);
        }

        printf("GPIO Taxi Task Exit\n");
        return NULL;
    }
#endif
