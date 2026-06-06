/**
 * @file can_parser.cc
 * @author Toan Vo <toan.vo@skyautonet.com>
 * @brief Parsing CAN messages from TATADAEWOO
 * @copyright Copyright (c) 2024 SkyAutonet Inc. All rights reserved.
 */

 #include "can_parser.h"
 #include "aivmculib.h"
 #include "stm32g4xx_can_def.h"
 #include "can_parser_tatamotors_ebus.h"
 
 #include <chrono>
 #include <algorithm>
 
 using namespace AIVMCULib;
 
 static void can_parser_18FEF117 (const uint8_t*, VehicleInfo_t &);
 static void can_parser_14FF1727 (const uint8_t*, VehicleInfo_t &);
 static void can_parser_18F01DE4 (const uint8_t*, VehicleInfo_t &);
 static void can_parser_18FF6221 (const uint8_t*, VehicleInfo_t &);
 
 #define REGISTER_CAN_PARSER_ID(can_id) m_parser_map.emplace(0x##can_id, &can_parser_##can_id);
 
AIVCANParserTataMotorsEbus::AIVCANParserTataMotorsEbus(const char* dev_name) : AIVCANParser(dev_name)
 {
   REGISTER_CAN_PARSER_ID(18FEF117);
   REGISTER_CAN_PARSER_ID(14FF1727);
   REGISTER_CAN_PARSER_ID(18F01DE4);
   REGISTER_CAN_PARSER_ID(18FF6221);
 }
 
 AIVCANParserTataMotorsEbus::~AIVCANParserTataMotorsEbus()
 {
 }
 
 void AIVCANParserTataMotorsEbus::getCanIdFilterCfg(std::vector<Cmd_CanBusFilterCfg_t> & cfg)
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
 
 int AIVCANParserTataMotorsEbus::parse(const CanFrame_t &frm, VehicleInfo_t &veh_inf)
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
 
 // TATA MOTORS ELECTRIC BUS
 
 static void can_parser_18FEF117 (const uint8_t* data, VehicleInfo_t &veh_inf)
 {
   // speed (km/h)
   // 2.1; 16 bits; 1/256 per bit; offset=0
   const int val = data[0] + data[1]*256;
   if(val != 0xFFFF) {
     veh_inf.speed_kmh = val / 256;
   }
 }
 
 static void can_parser_14FF1727 (const uint8_t* data, VehicleInfo_t &veh_inf)
 {
   // gear
   // Neutral: 3.2; 1 bits; 1 per bit; offset=0
   if(data[2] & (1<<1)) {
     veh_inf.gear = 0;
     return;
   }
   // Drive: 3.1; 1 bits; 1 per bit; offset=0
   if(data[2] & (1<<0)) {
     veh_inf.gear = 1;
     return;
   }
   // Reverse: 3.3; 1 bits; 1 per bit; offset=0
   if(data[2] & (1<<2)) {
     veh_inf.gear = -1;
     return;
   }
   // Power Mode (Parking?): 7.8; 1 bits; 1 per bit; offset=0
   if(data[6] & (1<<7)) {
     veh_inf.gear = 126;
     return;
   }
 }
 
 static void can_parser_18F01DE4 (const uint8_t* data, VehicleInfo_t &veh_inf)
 {
   // Steering Wheel Angle
   // 1.1; 16 bits; 1/1024 rad per bit; offset=-26.878 rad
   const int val = data[0] + data[1]*256;
   if(val != 0xFFFF) {
     veh_inf.str_whl_rad = (val/1024.f) - 26.878f;
   }
 }
 
 static void can_parser_18FF6221 (const uint8_t* data, VehicleInfo_t &veh_inf)
 {
   // turn signal
   // 1.1; 3 bits; 1 state/bit, offset=0
   // 000- OFF
   // 001- TurnRight (82 Flashes \Tictock Sound Per Minute) 
   // 010- TurnLeft (82 Flashes \Tictock Sound Per Minute )
   // 011- Turn Left double flash (152 Flashes\TiKtock Sound Per Minute )
   // 100- Turn Right double flash  (152 Flashes\TiKtock Sound Per Minute )
   // 101- Hazard (82 Flashes \Tictock Sound Per Minute for both tell tales)
   // 110- Reserve 
   // 111 - Reserve
   const int val = data[1] & 0x07;
   if(val == 0) {
     veh_inf.turn_signal = 0;
     return;
   }
   if(val == 0b001 || val == 0b100) {
     veh_inf.turn_signal = 2;
     return;
   }
   if(val == 0b010 || val == 0b011) {
     veh_inf.turn_signal = 1;
     return;
   }
 }