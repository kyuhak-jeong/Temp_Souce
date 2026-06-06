/**
  ******************************************************************************
  * @file    stm32g4xx_hal_fdcan.h
  * @author  MCD Application Team
  * @brief   Header file of FDCAN HAL module.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
 
#ifndef STM32G4xx_CAN_DEF_H
#define STM32G4xx_CAN_DEF_H

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup FDCAN_clock_divider FDCAN Clock Divider
  * @{
  */
#define FDCAN_CLOCK_DIV1  ((uint32_t)0x00000000U) /*!< Divide kernel clock by 1  */
#define FDCAN_CLOCK_DIV2  ((uint32_t)0x00000001U) /*!< Divide kernel clock by 2  */
#define FDCAN_CLOCK_DIV4  ((uint32_t)0x00000002U) /*!< Divide kernel clock by 4  */
#define FDCAN_CLOCK_DIV6  ((uint32_t)0x00000003U) /*!< Divide kernel clock by 6  */
#define FDCAN_CLOCK_DIV8  ((uint32_t)0x00000004U) /*!< Divide kernel clock by 8  */
#define FDCAN_CLOCK_DIV10 ((uint32_t)0x00000005U) /*!< Divide kernel clock by 10 */
#define FDCAN_CLOCK_DIV12 ((uint32_t)0x00000006U) /*!< Divide kernel clock by 12 */
#define FDCAN_CLOCK_DIV14 ((uint32_t)0x00000007U) /*!< Divide kernel clock by 14 */
#define FDCAN_CLOCK_DIV16 ((uint32_t)0x00000008U) /*!< Divide kernel clock by 16 */
#define FDCAN_CLOCK_DIV18 ((uint32_t)0x00000009U) /*!< Divide kernel clock by 18 */
#define FDCAN_CLOCK_DIV20 ((uint32_t)0x0000000AU) /*!< Divide kernel clock by 20 */
#define FDCAN_CLOCK_DIV22 ((uint32_t)0x0000000BU) /*!< Divide kernel clock by 22 */
#define FDCAN_CLOCK_DIV24 ((uint32_t)0x0000000CU) /*!< Divide kernel clock by 24 */
#define FDCAN_CLOCK_DIV26 ((uint32_t)0x0000000DU) /*!< Divide kernel clock by 26 */
#define FDCAN_CLOCK_DIV28 ((uint32_t)0x0000000EU) /*!< Divide kernel clock by 28 */
#define FDCAN_CLOCK_DIV30 ((uint32_t)0x0000000FU) /*!< Divide kernel clock by 30 */

/** @defgroup FDCAN_frame_format FDCAN Frame Format
  * @{
  */
#define FDCAN_FRAME_CLASSIC   ((uint32_t)0x00000000U)                         /*!< Classic mode                      */
#define FDCAN_FRAME_FD_NO_BRS ((uint32_t)FDCAN_CCCR_FDOE)                     /*!< FD mode without BitRate Switching */
#define FDCAN_FRAME_FD_BRS    ((uint32_t)(FDCAN_CCCR_FDOE | FDCAN_CCCR_BRSE)) /*!< FD mode with BitRate Switching    */

/** @defgroup FDCAN_operating_mode FDCAN Operating Mode
  * @{
  */
#define FDCAN_MODE_NORMAL               ((uint32_t)0x00000000U) /*!< Normal mode               */
#define FDCAN_MODE_BUS_MONITORING       ((uint32_t)0x00000002U) /*!< Bus Monitoring mode       */

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

/** @defgroup FDCAN_filter_config FDCAN Filter Configuration
  * @{
  */
#define FDCAN_FILTER_DISABLE       ((uint32_t)0x00000000U) /*!< Disable filter element                                    */
#define FDCAN_FILTER_TO_RXFIFO0    ((uint32_t)0x00000001U) /*!< Store in Rx FIFO 0 if filter matches                      */
#define FDCAN_FILTER_TO_RXFIFO1    ((uint32_t)0x00000002U) /*!< Store in Rx FIFO 1 if filter matches                      */
#define FDCAN_FILTER_REJECT        ((uint32_t)0x00000003U) /*!< Reject ID if filter matches                               */
#define FDCAN_FILTER_HP            ((uint32_t)0x00000004U) /*!< Set high priority if filter matches                       */
#define FDCAN_FILTER_TO_RXFIFO0_HP ((uint32_t)0x00000005U) /*!< Set high priority and store in FIFO 0 if filter matches   */
#define FDCAN_FILTER_TO_RXFIFO1_HP ((uint32_t)0x00000006U) /*!< Set high priority and store in FIFO 1 if filter matches   */

#ifdef __cplusplus
}
#endif

#endif /* STM32G4xx_CAN_DEF_H */
