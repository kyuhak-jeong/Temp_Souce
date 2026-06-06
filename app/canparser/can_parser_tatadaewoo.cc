/**
 * @file can_parser_tatadaewoo.cc
 * @author Toan Vo <toan.vo@skyautonet.com>
 * @brief Parsing CAN messages from TATADAEWOO
 * @copyright Copyright (c) 2024 SkyAutonet Inc. All rights reserved.
 */

#include "can_parser.h"
#include "aivmculib.h"
#include "stm32g4xx_can_def.h"
#include "can_parser_tatadaewoo.h"

#include <chrono>
#include <algorithm>

using namespace AIVMCULib;

static void can_parser_18F00503 (const uint8_t*, VehicleInfo_t &);
static void can_parser_0CFDCC27 (const uint8_t*, VehicleInfo_t &);
static void can_parser_000001E5 (const uint8_t*, VehicleInfo_t &);
static void can_parser_18FEF100 (const uint8_t*, VehicleInfo_t &);
static void can_parser_0C040B2A (const uint8_t*, VehicleInfo_t &);
static void can_parser_0C010005 (const uint8_t*, VehicleInfo_t &);
static void can_parser_18F0090B (const uint8_t*, VehicleInfo_t &);

#define REGISTER_CAN_PARSER_ID(can_id) m_parser_map.emplace(0x##can_id, &can_parser_##can_id);

AIVCANParserTataDaewoo::AIVCANParserTataDaewoo(const char* dev_name) : AIVCANParser(dev_name)
{
  REGISTER_CAN_PARSER_ID(18F00503);
  REGISTER_CAN_PARSER_ID(0CFDCC27);
  REGISTER_CAN_PARSER_ID(000001E5);
  REGISTER_CAN_PARSER_ID(18F0090B);
  REGISTER_CAN_PARSER_ID(18FEF100);
  REGISTER_CAN_PARSER_ID(0C040B2A);
  REGISTER_CAN_PARSER_ID(0C010005);
}

AIVCANParserTataDaewoo::~AIVCANParserTataDaewoo()
{
}

void AIVCANParserTataDaewoo::getCanIdFilterCfg(std::vector<Cmd_CanBusFilterCfg_t> & cfg)
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

int AIVCANParserTataDaewoo::parse(const CanFrame_t &frm, VehicleInfo_t &veh_inf)
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

// TATA DAEWOO

static void can_parser_18F00503 (const uint8_t* data, VehicleInfo_t &veh_inf)
{
  // gear
  // 4; 8 bits; 1 gear value/bit, offset=-125
  // invalid: 0xFF
  veh_inf.gear = data[3]-125;
}

static void can_parser_0CFDCC27 (const uint8_t* data, VehicleInfo_t &veh_inf)
{
  // turn signal
  // 2,1; 4 bits; 1 state/bit, offset=0
  veh_inf.turn_signal = data[1] & 0x0F;
}

static void can_parser_000001E5 (const uint8_t* data, VehicleInfo_t &veh_inf)
{
  // steering wheel
  // 2-3; 16 bits; big-endian; 16 degree/bit; offset=-2048 deg
  // Steering Wheel Angle Validity: bit 0, 1 bits, 0: valid, 1: invalid
  if((data[0]&1)==0)
  {
    const int val = data[2]*256 + data[1];
    veh_inf.str_whl_rad = ((val/16.f) - 2048)*0.01745329; // 1° × π/180 = 0.01745329rad
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

static void can_parser_0C040B2A (const uint8_t* data, VehicleInfo_t &veh_inf)
{
  // ACC
  static auto last_acc_chk = std::chrono::steady_clock::time_point();

  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::steady_clock::now( ) - last_acc_chk);
  if(elapsed.count() > 100) {
    veh_inf.acc = 0;
  } else {
    veh_inf.acc = 1;
  }

  last_acc_chk = std::chrono::steady_clock::now( );
}

static void can_parser_0C010005 (const uint8_t* data, VehicleInfo_t &veh_inf)
{
  // door
  // 6,1; 2 bits, 4 states/2 bit, offset=0
  veh_inf.door_open = data[5] & 0x03;
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