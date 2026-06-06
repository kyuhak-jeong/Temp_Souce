/**
 * @file can_parser.h
 * @author Toan Vo <toan.vo@skyautonet.com>
 * @brief
 * @copyright Copyright (c) 2024 SkyAutonet Inc. All rights reserved.
 */

#pragma once

#include "aivmculib.h"
#include "cmd_if.h"

#include <sys/ioctl.h> // SIOCDEVPRIVATE
#include <unordered_map>
#include <thread>
#include <mutex>

typedef void (*CANMsgParser)(const uint8_t*, AIVMCULib::VehicleInfo_t &);

class AIVCANParser
{
public:
  AIVCANParser(const char*);
  virtual ~AIVCANParser();
  virtual int parse(const AIVMCULib::CanFrame_t &, AIVMCULib::VehicleInfo_t &) = 0;
  virtual void getCanIdFilterCfg(std::vector<Cmd_CanBusFilterCfg_t> &)=0;
  void start();
  void stop();
  int getVehicleInfo(AIVMCULib::VehicleInfo_t&);

  enum Gear {
    REVERSE = -1,
    NEUTRAL = 0,
    DRIVING = 1,
    PARKING = 126,
  };

  enum DoorSts {
    OPEN = 0,
    CLOSED = 1,
  };

private:
  const char* m_dev_name;
  std::thread m_parser_thread;
  bool m_is_running{true};
  std::mutex m_veh_inf_mtx;
  AIVMCULib::VehicleInfo_t m_vehicle_inf{0};
  bool m_vehicle_info_updated{false};

private:
  void CANParserThread();

protected:
  std::unordered_map<canid_t, CANMsgParser> m_parser_map;
};

