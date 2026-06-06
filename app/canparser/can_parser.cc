/**
 * @file can_parser.cc
 * @author Toan Vo <toan.vo@skyautonet.com>
 * @brief
 * @copyright Copyright (c) 2024 SkyAutonet Inc. All rights reserved.
 */

#include "can_parser.h"
#include "aivmculib.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cerrno>
#include <chrono>
#include <stdexcept>
#include <algorithm>
#include <thread>
#include <unistd.h>
#include <fcntl.h>
#include <linux/can.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/can/raw.h>

#define LOG_LVL PRINT_LOG_LVL_WARN
#include "log_print.h"
PRINT_SETTAG("can_parser");

/** @defgroup FDCAN_id_type FDCAN ID Type
  * @{
  */
#define FDCAN_STANDARD_ID ((uint32_t)0x00000000U) /*!< Standard ID element */
#define FDCAN_EXTENDED_ID ((uint32_t)0x40000000U) /*!< Extended ID element */

/** @defgroup FDCAN_filter_type FDCAN Filter Type
  * @{
  */
#define FDCAN_FILTER_RANGE         ((uint32_t)0x00000000U) /*!< Range filter from FilterID1 to FilterID2                        */
#define FDCAN_FILTER_DUAL          ((uint32_t)0x00000001U) /*!< Dual ID filter for FilterID1 or FilterID2                       */
#define FDCAN_FILTER_MASK          ((uint32_t)0x00000002U) /*!< Classic filter: FilterID1 = filter, FilterID2 = mask            */
#define FDCAN_FILTER_RANGE_NO_EIDM ((uint32_t)0x00000003U) /*!< Range filter from FilterID1 to FilterID2, EIDM mask not applied */

#define IOCTL_MAGIC     'M'
#define IOCTL_READ_VER  _IOR(IOCTL_MAGIC, 0, uint32_t)
#define CAN_IOCTL_SET_CAN_FILTER  (SIOCDEVPRIVATE)
#define CAN_IOCTL_RESET_CAN_FILTER  (SIOCDEVPRIVATE+1)

#define DUMMY_MCUCOMM 1

using namespace AIVMCULib;

AIVCANParser::AIVCANParser(const char* dev_name):
  m_dev_name(dev_name)
{
  printf("dev_name: %s\n", dev_name);
}

AIVCANParser::~AIVCANParser()
{
  m_parser_map.clear();
}

void AIVCANParser::start()
{
  PRINT_WARN("%s", __func__);
  m_parser_thread = std::thread(&AIVCANParser::CANParserThread, this);
}

void AIVCANParser::stop()
{
  PRINT_WARN("%s", __func__);
  m_is_running = false;
  if(m_parser_thread.joinable()) {
    m_parser_thread.join();
  }
}

int AIVCANParser::getVehicleInfo(AIVMCULib::VehicleInfo_t &veh_inf)
{
  if(!m_vehicle_info_updated) {
    return -ENODATA;
  }
  m_veh_inf_mtx.lock();
  veh_inf = m_vehicle_inf;
  m_veh_inf_mtx.unlock();

  return 0;
}

void AIVCANParser::CANParserThread()
{
  PRINT_INFO("enter");
  struct ifreq ifr;
  struct sockaddr_can addr;
  int sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
  addr.can_family = AF_CAN;
  strncpy(ifr.ifr_name, m_dev_name, IFNAMSIZ);
  if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0)
  {
    PRINT_ERR("No CAN device found: %s", strerror(errno));
    close(sock);
    throw std::runtime_error("[can_parser] no CAN device found");
    return;
  }
  addr.can_ifindex = ifr.ifr_ifindex;
  fcntl(sock, F_SETFL, O_NONBLOCK);
  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
  {
    PRINT_ERR("CAN Socket binding failed: %s", strerror(errno));
    close(sock);
    throw std::runtime_error("[can_parser] CAN Socket binding failed");
    return;
  }

#if !DUMMY_MCUCOMM
  if(ioctl(sock, CAN_IOCTL_RESET_CAN_FILTER, &ifr)) {
    PRINT_INFO("ioctl: Reset CAN ID filtering %d", errno);
  }
#endif // DUMMY_MCUCOMM

  // filtering Rx CAN messages
  std::vector<Cmd_CanBusFilterCfg_t> filter_cfg;
  getCanIdFilterCfg(filter_cfg);
  if(!filter_cfg.empty())
  {
    const int nn = filter_cfg.size();
    struct can_filter rfilter[nn*2];
    int cnt = 0;
    for(int i=0; i<nn; ++i)
    {
      const auto &cmd = filter_cfg[i];
      if(cmd.FilterType == FDCAN_FILTER_MASK) {
        rfilter[cnt].can_id = cmd.FilterID1;
        rfilter[cnt].can_mask = cmd.FilterID2;
        ++cnt;
      } else if(cmd.FilterType == FDCAN_FILTER_DUAL) {
        const uint32_t can_mask = (cmd.IdType==FDCAN_STANDARD_ID)? 0x7FF : 0x1FFFFFFF;
        rfilter[cnt].can_id = cmd.FilterID1;
        rfilter[cnt].can_mask = can_mask;
        rfilter[cnt+1].can_id = cmd.FilterID2;
        rfilter[cnt+1].can_mask = can_mask;
        cnt+=2;
      }

      ifr.ifr_data = (char*)&cmd;

#if !DUMMY_MCUCOMM
      if(ioctl(sock, CAN_IOCTL_SET_CAN_FILTER, &ifr)) {
        PRINT_WARN("ioctl CAN ID filtering %d", errno);
      }
#endif // DUMMY_MCUCOMM
    }

    setsockopt(sock, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter[0])*cnt);
  }
  filter_cfg.clear();

  struct can_frame frame_rd;
  VehicleInfo_t veh_inf = {
    .gear         = 0xff,
    .turn_signal  = 0xff,
    .str_whl_rad  = 0.0f,
    .speed_kmh    = 0,
    .door_lock    = 0xff,
    .acc          = 0xff,
    .door_open    = 0xff,
    .ext_temp     = 0xff,
    .door_trim_btn = 0xff,
    .maneuvering_view = 0xff,
    .rpm          = 0xff,
  };
  m_vehicle_inf = veh_inf;

  while (m_is_running)
  {
    //NOTE(toan): this is non-blocking call due to the socket setting
    ssize_t recvbytes = read(sock, &frame_rd, sizeof(frame_rd));

    if(recvbytes > 0) {
      if(parse(frame_rd, veh_inf)) {
        // update vehicle info
        const auto tp = std::chrono::system_clock::now();
        const std::time_t t = std::chrono::system_clock::to_time_t(tp);
        std::tm* now = std::gmtime(&t);
        veh_inf.dt.year = now->tm_year + 1900;
        veh_inf.dt.month = now->tm_mon + 1;
        veh_inf.dt.day = now->tm_mday;
        veh_inf.dt.hours = now->tm_hour;
        veh_inf.dt.mins = now->tm_min;
        veh_inf.dt.secs = now->tm_sec;
        veh_inf.dt.milisecs = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count()%1000;
        m_veh_inf_mtx.lock();
        m_vehicle_inf = veh_inf;
        m_veh_inf_mtx.unlock();
        m_vehicle_info_updated = true;
      }
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  close(sock);

  PRINT_INFO("stopped");
}
