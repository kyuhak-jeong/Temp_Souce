/**
 * @file can_parser_tatadaewoo.h
 * @author Toan Vo <toan.vo@skyautonet.com>
 * @brief Parsing CAN messages from TATADAEWOO
 * @copyright Copyright (c) 2024 SkyAutonet Inc. All rights reserved.
 */
#pragma once

#include "can_parser.h"

class AIVCANParserTataDaewoo : public AIVCANParser
{
public:
  AIVCANParserTataDaewoo(const char*);
  ~AIVCANParserTataDaewoo();
  int parse(const AIVMCULib::CanFrame_t &, AIVMCULib::VehicleInfo_t &);
  void getCanIdFilterCfg(std::vector<Cmd_CanBusFilterCfg_t> &);
};
