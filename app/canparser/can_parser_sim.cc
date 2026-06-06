/**
 * @file can_parser_sim.cc
 * @author Toan Vo <toan.vo@skyautonet.com>
 * @brief Parsing CAN messages from Vehicle simulator board (modified SeDA JIG)
 * @copyright Copyright (c) 2024 SkyAutonet Inc. All rights reserved.
 */

#include "can_parser.h"
#include "aivmculib.h"
#include "stm32g4xx_can_def.h"
#include "can_parser_sim.h"

#include <chrono>
#include <algorithm>

using namespace AIVMCULib;

static void can_parser_100 (const uint8_t*, VehicleInfo_t &);

#define REGISTER_CAN_PARSER_ID(can_id) m_parser_map.emplace(0x##can_id, &can_parser_##can_id);

AIVCANParserSim::AIVCANParserSim(const char* dev_name) : AIVCANParser(dev_name)
{
  REGISTER_CAN_PARSER_ID(100);
}

AIVCANParserSim::~AIVCANParserSim()
{
}

void AIVCANParserSim::getCanIdFilterCfg(std::vector<Cmd_CanBusFilterCfg_t> & cfg)
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

int AIVCANParserSim::parse(const CanFrame_t &frm, VehicleInfo_t &veh_inf)
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

static void can_parser_100 (const uint8_t* data, VehicleInfo_t &veh_inf)
{
  const int rpm = data[0] + data[1]*256;
  const int speed = data[2];
  const int steering_wheel_angle = (data[3]+data[4]*256)-500;
  const int door_state = (data[5] >> 7) & 1;
  const int left_turn_signal = (data[5] >> 6) & 1;
  const int right_turn_signal = (data[5] >> 5) & 1;
  const int monitor_btn = (data[5] >> 4) & 1;
  const int gear_pos = data[5] & 0x07;
  veh_inf.rpm = rpm;
  veh_inf.speed_kmh = speed;
  veh_inf.str_whl_rad = steering_wheel_angle*0.0174533f;
  veh_inf.door_open = (door_state==0)?1:0;
  veh_inf.turn_signal = left_turn_signal | (right_turn_signal<<1);
  veh_inf.gear = gear_pos?1:126;
  veh_inf.monitor_btn = monitor_btn;
}
