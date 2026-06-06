/**
 * @file can_parser_kgm_electric_bus.cc
 * @author Toan Vo <toan.vo@skyautonet.com>
 * @brief Parsing CAN messages from KGM Electric Bus
 * @copyright Copyright (c) 2024 SkyAutonet Inc. All rights reserved.
 */

#include "can_parser.h"
#include "aivmculib.h"
#include "stm32g4xx_can_def.h"
#include "can_parser_kgm_electric_bus.h"

#include <chrono>
#include <algorithm>

using namespace AIVMCULib;

static void can_parser_0C010305 (const uint8_t*, VehicleInfo_t &);
static void can_parser_0CFF2127 (const uint8_t*, VehicleInfo_t &);

#define REGISTER_CAN_PARSER_ID(can_id) m_parser_map.emplace(0x##can_id, &can_parser_##can_id);

AIVCANParserKgmEBus::AIVCANParserKgmEBus(const char* dev_name) : AIVCANParser(dev_name)
{
  REGISTER_CAN_PARSER_ID(0C010305);
  REGISTER_CAN_PARSER_ID(0CFF2127);
}

AIVCANParserKgmEBus::~AIVCANParserKgmEBus()
{
}

void AIVCANParserKgmEBus::getCanIdFilterCfg(std::vector<Cmd_CanBusFilterCfg_t> & cfg)
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

int AIVCANParserKgmEBus::parse(const CanFrame_t &frm, VehicleInfo_t &veh_inf)
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

static void can_parser_0C010305 (const uint8_t* data, VehicleInfo_t &veh_inf)
{
  veh_inf.gear = data[3];
}

static void can_parser_0CFF2127 (const uint8_t* data, VehicleInfo_t &veh_inf)
{
  // speed
  veh_inf.speed_kmh = data[0]*256 + data[1];
}
