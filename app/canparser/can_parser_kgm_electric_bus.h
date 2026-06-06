/**
 * @file can_parser_kgm_electric_bus.h
 * @author Toan Vo <toan.vo@skyautonet.com>
 * @brief Parsing CAN messages from KGM Electric Bus
 * @copyright Copyright (c) 2024 SkyAutonet Inc. All rights reserved.
 */
#pragma once

#include "can_parser.h"

class AIVCANParserKgmEBus : public AIVCANParser
{
public:
  AIVCANParserKgmEBus(const char*);
  ~AIVCANParserKgmEBus();
  int parse(const AIVMCULib::CanFrame_t &, AIVMCULib::VehicleInfo_t &);
  void getCanIdFilterCfg(std::vector<Cmd_CanBusFilterCfg_t> &);
};