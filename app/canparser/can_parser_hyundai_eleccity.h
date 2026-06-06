/**
 * @file can_parser_hyundai_eleccity.h
 * @author Toan Vo <toan.vo@skyautonet.com>
 * @brief Parsing CAN messages from Hyundai ElecCity Bus
 * @copyright Copyright (c) 2024 SkyAutonet Inc. All rights reserved.
 */
#pragma once

#include "can_parser.h"

class AIVCANParserHyundaiElecCity : public AIVCANParser
{
public:
  AIVCANParserHyundaiElecCity(const char*);
  ~AIVCANParserHyundaiElecCity();
  int parse(const AIVMCULib::CanFrame_t &, AIVMCULib::VehicleInfo_t &);
  void getCanIdFilterCfg(std::vector<Cmd_CanBusFilterCfg_t> &);
};
