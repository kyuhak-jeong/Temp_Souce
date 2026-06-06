/**
 * @file can_parser_kia_bongo3ev.cc
 * @author Toan Vo <toan.vo@skyautonet.com>
 * @brief Parsing CAN messages from Kia Bongo3 EV
 * @copyright Copyright (c) 2024 SkyAutonet Inc. All rights reserved.
 */

#include "can_parser.h"
#include "aivmculib.h"
#include "stm32g4xx_can_def.h"
#include "can_parser_kia_bongo3ev.h"

#include <chrono>
#include <algorithm>

using namespace AIVMCULib;

static void can_parser_372 (const uint8_t*, VehicleInfo_t &);
static void can_parser_2B0 (const uint8_t*, VehicleInfo_t &);
static void can_parser_52A (const uint8_t*, VehicleInfo_t &);
static void can_parser_541 (const uint8_t*, VehicleInfo_t &);

#define REGISTER_CAN_PARSER_ID(can_id) m_parser_map.emplace(0x##can_id, &can_parser_##can_id);

AIVCANParserKiaBongo3Ev::AIVCANParserKiaBongo3Ev(const char* dev_name) : AIVCANParser(dev_name)
{
  REGISTER_CAN_PARSER_ID(372);
  REGISTER_CAN_PARSER_ID(2B0);
  REGISTER_CAN_PARSER_ID(52A);
  REGISTER_CAN_PARSER_ID(541);
}

AIVCANParserKiaBongo3Ev::~AIVCANParserKiaBongo3Ev()
{
}

void AIVCANParserKiaBongo3Ev::getCanIdFilterCfg(std::vector<Cmd_CanBusFilterCfg_t> & cfg)
{
  std::vector<uint32_t> parser_ids;
  for(auto m : m_parser_map) {
    parser_ids.push_back(m.first);
  }
  const int nn = parser_ids.size();

  for(int i=0; i<nn; i+=2)
  {
    Cmd_CanBusFilterCfg_t cmd = {};
    cmd.IdType = FDCAN_STANDARD_ID;
    cmd.FilterIndex = i;
    cmd.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    cmd.FilterType = FDCAN_FILTER_MASK;
    cmd.FilterID1 = parser_ids[i];
    cmd.FilterID2 = 0x7FF;
    if((i+1) < nn) {
      cmd.FilterType = FDCAN_FILTER_DUAL;
      cmd.FilterID2 = parser_ids[i+1];
    }
    cfg.push_back(cmd);
  }
}

int AIVCANParserKiaBongo3Ev::parse(const CanFrame_t &frm, VehicleInfo_t &veh_inf)
{
  int is_updated = 0;

  if((frm.can_id & CAN_EFF_FLAG) != 0) {
    return is_updated;
  }

  auto iter = m_parser_map.find(frm.can_id & CAN_SFF_MASK);
  if (iter != m_parser_map.end()) {
    (*iter->second)((uint8_t*)frm.data, veh_inf);
    is_updated = 1;
  }

  return is_updated;
}

static void can_parser_372 (const uint8_t* data, VehicleInfo_t &veh_inf)
{
  // Elect_Gear_Shifter
  // 16|3@1+ (1,0) [0|7] ""  CLU
  // P:0, D:5, N:6, R:7
  const int val = data[2] & 0x07;
  switch(val)
  {
    case 0: // P
      veh_inf.gear = AIVCANParser::Gear::PARKING;
      break;
    case 5: // D
      veh_inf.gear = AIVCANParser::Gear::DRIVING;
      break;
    case 6: // N
      veh_inf.gear = AIVCANParser::Gear::NEUTRAL;
      break;
    case 7: // R
      veh_inf.gear = AIVCANParser::Gear::REVERSE;
      break;
  }
}

static void can_parser_2B0 (const uint8_t* data, VehicleInfo_t &veh_inf)
{
  // SAS_Angle: 0|16@1- (0.1,0.0) [-3276.8|3276.7] "Deg"
  const int val = (int16_t)(data[0] + data[1]*256);
  veh_inf.str_whl_rad = val*0.1*0.0174533;
}

static void can_parser_52A (const uint8_t* data, VehicleInfo_t &veh_inf)
{
  // CF_Clu_VehicleSpeed: 0|8@1+ (1.0,0.0) [0.0|255.0] "km/h"
  veh_inf.speed_kmh = data[0];
}

static void can_parser_541 (const uint8_t* data, VehicleInfo_t &veh_inf)
{
  // 운전석 도어 열림/닫힘 : 8|1@1+ (1.0,0.0) [0.0|1.0] "닫힘:0, 열림:1"
  // 조수석 도어 열림/닫힘 : 36|1@1+ (1.0,0.0) [0.0|1.0] ""
  const int val = (data[1] & 0x01) | ((data[4]>>4) & 0x01);
  veh_inf.door_open = val?AIVCANParser::DoorSts::OPEN:AIVCANParser::DoorSts::CLOSED;
  // 좌측 깜박이 램프 : 19|1@1+ (1.0,0.0) [0.0|1.0]
  // 우측 깜박이 램프 : 62|1@1+ (1.0,0.0) [0.0|2.0]
  veh_inf.turn_signal = ((data[2]>>3) & 0x01) | ((data[7]>>5) & 0x02);
}
