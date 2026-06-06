/**
 * @file can_parser_hyundai_new_county.h
 * @author Toan Vo <toan.vo@skyautonet.com>
 * @brief Parsing CAN messages from Hyundai New County (2019)
 * @copyright Copyright (c) 2024 SkyAutonet Inc. All rights reserved.
 */
#pragma once

#include "can_parser.h"

class AIVCANParserHyundaiNewCounty : public AIVCANParser
{
public:
  AIVCANParserHyundaiNewCounty(const char*);
  ~AIVCANParserHyundaiNewCounty();
  int parse(const AIVMCULib::CanFrame_t &, AIVMCULib::VehicleInfo_t &);
  void getCanIdFilterCfg(std::vector<Cmd_CanBusFilterCfg_t> &);
};