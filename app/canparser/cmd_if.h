/**
 * @file cmd_if.h
 * @author Toan Vo <toan.vo@skyautonet.com>
 * @brief
 * @copyright Copyright (c) 2023 SkyAutonet Inc. All rights reserved.
 */

#ifndef CMD_MGR_CMD_IF_H_
#define CMD_MGR_CMD_IF_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    MSG_TYPE_CMD = 1,
    MSG_TYPE_RES = 1,
    MSG_TYPE_EVT = 2,
};

enum {
    CMD_CANBUS_INIT = 1,
    CMD_CANBUS_FILTERCFG,
    CMD_CANBUS_START,
    CMD_CANBUS_STOP,
    CMD_CANBUS_MSG,
    CMD_FWUP_START,
    CMD_FWUP_DATA,
    CMD_FWUP_STOP,
    CMD_SOC_PWROFF,
    CMD_SOC_PWRRST,
    CMD_SET_TIME,
    CMD_FW_INFO,
    CMD_CANBUS_RESET_FILTER,
    CMD_BUZZER_CTRL,
};

enum {
    EVT_CANBUS_MSG = 1,
    EVT_MCU_STS,
};

enum {
    E_OK = 0,
    E_UNSUP,
    E_INVAL,
    E_FAILED,
    E_CRC,
};

#pragma pack(push, 1)

typedef struct {
    uint8_t CmdType;  /// 1: Command/Response, 2:Event
    uint8_t CmdId;
    uint16_t PayloadLen;
    uint8_t Payload[];
} Msg_Hdr_t;

typedef struct {
    uint8_t Result;
} Msg_Resp_t;

// FDCAN_FilterTypeDef
typedef struct {
    uint32_t BusId;
    uint32_t IdType;
    uint32_t FilterIndex;
    uint32_t FilterType;
    uint32_t FilterConfig;
    uint32_t FilterID1;
    uint32_t FilterID2;
} Cmd_CanBusFilterCfg_t;

typedef struct {
    uint32_t Result;
    uint32_t BusId;
} Resp_CanBusFilterCfg_t;

// FDCAN_InitTypeDef
typedef struct {
    uint32_t BusId;
    uint32_t ClockDivider;
    uint32_t FrameFormat;
    uint32_t Mode;
    uint32_t NominalPrescaler;
    uint32_t NominalSyncJumpWidth;
    uint32_t NominalTimeSeg1;
    uint32_t NominalTimeSeg2;
    uint32_t DataPrescaler;
    uint32_t DataSyncJumpWidth;
    uint32_t DataTimeSeg1;
    uint32_t DataTimeSeg2;
    uint32_t StdFiltersNbr;
    uint32_t ExtFiltersNbr;
} Cmd_CanBusInit_t;

typedef struct {
    uint32_t Result;
    uint32_t BusId;
} Resp_CanBusInit_t;

typedef struct {
    uint32_t BusId;
} Cmd_CanBusStart_t;

typedef struct {
    uint32_t Result;
    uint32_t BusId;
} Resp_CanBusStart_t;

typedef struct {
    uint32_t BusId;
} Cmd_CanBusStop_t;

typedef struct {
    uint32_t Result;
    uint32_t BusId;
} Resp_CanBusStop_t;

typedef struct {
    uint32_t BusId;
    uint32_t CanID;
    uint8_t Length;
    uint8_t Reserved[3];
    uint8_t Data[];
} Cmd_CanBusSendMsg_t;

typedef struct {
    uint32_t Result;
    uint32_t BusId;
} Resp_CanBusSendMsg_t;

typedef struct {
    uint32_t total_len;
    uint32_t crc;
} Cmd_FwUp_Start_t;

typedef struct {
    uint32_t offset;
    uint32_t len;
    uint8_t data[];
} Cmd_FwUp_Data_t;

typedef struct {
    uint32_t op;
} Cmd_BuzzerCtrl_t;

typedef struct {
    uint32_t Result;
    uint32_t Version;
} Resp_FwInfo_t;

typedef struct {
    uint32_t CanId;
    uint8_t BusId;
    uint8_t Len;
    uint8_t Reserved[2];
    uint8_t data[];
} Evt_CanMsg_t;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif /* CMD_MGR_CMD_IF_H_ */
