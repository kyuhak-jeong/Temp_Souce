/**
 * @file can_parser_tatamotors_ebus.h
 * @author Toan Vo <toan.vo@skyautonet.com>
 * @brief Parsing CAN messages from TATADAEWOO E-BUS (TML POC)
 * @copyright Copyright (c) 2025 SkyAutonet Inc. All rights reserved.
 */
#pragma once

#include "can_parser.h"

class AIVCANParserTataMotorsEbus : public AIVCANParser
{
public:
  AIVCANParserTataMotorsEbus(const char*);
  ~AIVCANParserTataMotorsEbus();
  int parse(const AIVMCULib::CanFrame_t &, AIVMCULib::VehicleInfo_t &);
  void getCanIdFilterCfg(std::vector<Cmd_CanBusFilterCfg_t> &);
};
