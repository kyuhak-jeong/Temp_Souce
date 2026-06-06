/**
 * @file can_parser_sim.h
 * @author Toan Vo <toan.vo@skyautonet.com>
 * @brief Parsing CAN messages from Vehicle simulator board (modified SeDA JIG)
 * @copyright Copyright (c) 2024 SkyAutonet Inc. All rights reserved.
 */
#pragma once

#include "can_parser.h"

class AIVCANParserSim : public AIVCANParser
{
public:
  AIVCANParserSim(const char*);
  ~AIVCANParserSim();
  int parse(const AIVMCULib::CanFrame_t &, AIVMCULib::VehicleInfo_t &);
  void getCanIdFilterCfg(std::vector<Cmd_CanBusFilterCfg_t> &);
};