/**
 * @file can_parser.cc
 * @author Toan Vo <toan.vo@skyautonet.com>
 * @brief Parsing CAN messages from Hyundai New County (2019)
 * @copyright Copyright (c) 2024 SkyAutonet Inc. All rights reserved.
 */

#include "can_parser.h"
#include "aivmculib.h"
#include "stm32g4xx_can_def.h"
#include "can_parser_hyundai_new_county.h"
 
#include <chrono>
#include <algorithm>

using namespace AIVMCULib;


static void can_parser_0CF00400 (const uint8_t*, VehicleInfo_t &); // RPM
static void can_parser_18F00503 (const uint8_t*, VehicleInfo_t &); // Gear
static void can_parser_0CFDCCE6 (const uint8_t*, VehicleInfo_t &); // Turn Signal
static void can_parser_18F0090B (const uint8_t*, VehicleInfo_t &); // Steering Wheel
static void can_parser_18FEF100 (const uint8_t*, VehicleInfo_t &); // Speed (OK)
static void can_parser_18FF2AEF (const uint8_t*, VehicleInfo_t &); // Door Open
static void can_parser_18FEF1EF (const uint8_t*, VehicleInfo_t &); // Door Close

#define REGISTER_CAN_PARSER_ID(can_id) m_parser_map.emplace(0x##can_id, &can_parser_##can_id);

AIVCANParserHyundaiNewCounty::AIVCANParserHyundaiNewCounty(const char* dev_name): AIVCANParser(dev_name)
{
  REGISTER_CAN_PARSER_ID(0CF00400);
  REGISTER_CAN_PARSER_ID(18F00503);
  REGISTER_CAN_PARSER_ID(0CFDCCE6);
  REGISTER_CAN_PARSER_ID(18F0090B);
  REGISTER_CAN_PARSER_ID(18FEF100);
  REGISTER_CAN_PARSER_ID(18FF2AEF);
  REGISTER_CAN_PARSER_ID(18FEF1EF);
}

AIVCANParserHyundaiNewCounty::~AIVCANParserHyundaiNewCounty()
{
}

void AIVCANParserHyundaiNewCounty::getCanIdFilterCfg(std::vector<Cmd_CanBusFilterCfg_t> & cfg)
{
  std::vector<uint32_t> parser_ids;
  for(auto m : m_parser_map) {
    parser_ids.push_back(m.first);
  }
  const int nn = parser_ids.size();

  for(int i=0; i<nn; i+=2)
  {
    Cmd_CanBusFilterCfg_t cmd = {};
    cmd.IdType = FDCAN_EXTENDED_ID;
    cmd.FilterIndex = i;
    cmd.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    cmd.FilterType = FDCAN_FILTER_MASK;
    cmd.FilterID1 = parser_ids[i];
    cmd.FilterID2 = 0x1FFFFFFF;
    if((i+1) < nn) {
      cmd.FilterType = FDCAN_FILTER_DUAL;
      cmd.FilterID2 = parser_ids[i+1];
    }
    cfg.push_back(cmd);
  }
}

int AIVCANParserHyundaiNewCounty::parse(const CanFrame_t &frm, VehicleInfo_t &veh_inf)
{
  int is_updated = 0;

  if((frm.can_id & CAN_EFF_FLAG) == 0) {
    return is_updated;
  }

  auto iter = m_parser_map.find(frm.can_id & CAN_EFF_MASK);
  if (iter != m_parser_map.end()) {
    (*iter->second)((uint8_t*)frm.data, veh_inf);
    is_updated = 1;
  }

  return is_updated;
}

static void can_parser_0CF00400 (const uint8_t* data, VehicleInfo_t &veh_inf)
{
  // RPM
  if ((data[3] != 0xff) || (data[4] != 0xff))
  {
    veh_inf.rpm = (data[3] + data[4]*256) * 0.125;
  }
}


static void can_parser_18F00503 (const uint8_t* data, VehicleInfo_t &veh_inf)
{
  // transmission selected gear
  // 1; 8 bits; 1 gear value/bit, offset=-125
  // invalid: 0xFF
  veh_inf.gear = data[0]-125;
}

static void can_parser_0CFDCCE6 (const uint8_t* data, VehicleInfo_t &veh_inf)
{
  // turn signal
  // 2,1; 4 bits; 1 state/bit, offset=0
  veh_inf.turn_signal = data[1] & 0x0F;
}

static void can_parser_18F0090B (const uint8_t* data, VehicleInfo_t &veh_inf)
{
  // steering wheel
  // 1-2; 16 bits; 1/1024 rad per bit; offset=-31.374 rad
  const int val = data[0] + data[1]*256;
  if(val != 0xFFFF) {
    veh_inf.str_whl_rad = (val/1024.f) - 31.374f;
  }
}

static void can_parser_18FEF100 (const uint8_t* data, VehicleInfo_t &veh_inf)
{
  // speed
  const int val = (data[1] + data[2]*256);
  if(val != 0xFFFF) {
    veh_inf.speed_kmh = val/256;
  }
}

static void can_parser_18FF2AEF (const uint8_t* data, VehicleInfo_t &veh_inf)
{
  // DoorSwOn
  // 2.8; 1 bit
  // 0x00:Close
  // 0x01:Open
  veh_inf.door_open = ((data[1]>>7) & 0x01)^1;
}

static void can_parser_18FEF1EF (const uint8_t* data, VehicleInfo_t &veh_inf)
{
  // DoorSW
  // 5.3; 2 bit
  // 0x0:Fr and Mid Door Switch Not Set
  // 0x1:Fr or Mid Switch Set
  // 0x2:Error
  // 0x3:Not available
  veh_inf.door_lock = (data[4]>>2) & 0x03;
}
