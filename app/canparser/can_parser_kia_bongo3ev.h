/**
 * @file can_parser_kia_bongo3ev.h
 * @author Toan Vo <toan.vo@skyautonet.com>
 * @brief Parsing CAN messages from Kia Bongo3 EV
 * @copyright Copyright (c) 2024 SkyAutonet Inc. All rights reserved.
 */
#pragma once

#include "can_parser.h"

class AIVCANParserKiaBongo3Ev : public AIVCANParser
{
public:
  AIVCANParserKiaBongo3Ev(const char*);
  ~AIVCANParserKiaBongo3Ev();
  int parse(const AIVMCULib::CanFrame_t &, AIVMCULib::VehicleInfo_t &);
  void getCanIdFilterCfg(std::vector<Cmd_CanBusFilterCfg_t> &);
};