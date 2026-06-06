#pragma once

#ifdef USE_CAN

#include <atomic>
#include <smallwin.hpp>
#include "can_parser.h"
#include "app_vars.h"

extern std::atomic<GEAR> globalGearState;

GEAR mapGearToStandard(int gear, int vehicle_flag);

int  CalculateTurnSignalInterval(int left, int right);

void Process_Vehicle_Speed_Status(int vehicle_speed, int gear_pos);
void Process_Vehicle_Canrx_Info(const AIVMCULib::VehicleInfo_t& veh_inf);
void Process_Vehicle_Canrx_Info(const AIVMCULib::VehicleInfo_t& veh_inf, const AIVMCULib::McuStatus_t& mcu_status);

#ifdef DEVICE_RK3588
    extern TaskT     m_Task_canrx;
    extern TaskAttrT m_tAttrTask_canrx;
    void* Canrx_RK3588_Task(void* pArg);
#endif

#ifdef DEVICE_RK3576
    extern TaskT     m_Task_canrx;
    extern TaskAttrT m_tAttrTask_canrx;
    void* CanRx_RK3576_Task(void* pArg);
#endif

#ifdef USE_TAXI
    extern TaskT     m_Task_gpio_taxi;
    extern TaskAttrT m_tAttrTask_gpio_taxi;
    void* GpioTaxi_Task(void* pArg);
#endif

#endif // USE_CAN