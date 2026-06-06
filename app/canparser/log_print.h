/**
 * @file log_print.h
 * @author Toan Vo <toan.vo@skyautonet.com>
 * @brief
 * @copyright Copyright (c) 2023 SkyAutonet Inc. All rights reserved.
 */

#pragma once

#include <cstdint>
#include <cstdio>

#define PRINT_CREATE_LOGGER(...)
#define PRINT_STOP_LOGGER(...)
#define PRINT_SETTAG(log_tag) static const char* LOG_TAG = log_tag
#define PRINT_LOG_LVL_TRACE 4
#define PRINT_LOG_LVL_DEBUG 3
#define PRINT_LOG_LVL_INFO  2
#define PRINT_LOG_LVL_WARN  1
#define PRINT_LOG_LVL_ERROR 0
#if LOG_LVL >= PRINT_LOG_LVL_TRACE
#define PRINT_TRACE(fmt, args...) fprintf(stdout, "<T> [%s] %s(%d) " fmt "\n", LOG_TAG, __func__, __LINE__, ## args)
#else
#define PRINT_TRACE(...)
#endif
#if LOG_LVL >= PRINT_LOG_LVL_DEBUG
#define PRINT_DEBUG(fmt, args...) fprintf(stdout, "<D> [%s] %s(%d) " fmt "\n", LOG_TAG, __func__, __LINE__, ## args)
#else
#define PRINT_DEBUG(...)
#endif
#if LOG_LVL >= PRINT_LOG_LVL_INFO
#define PRINT_INFO(fmt, args...)  fprintf(stdout, "<I> [%s] %s(%d) " fmt "\n", LOG_TAG, __func__, __LINE__, ## args)
#else
#define PRINT_INFO(...)
#endif
#if LOG_LVL >= PRINT_LOG_LVL_WARN
#define PRINT_WARN(fmt, args...)  fprintf(stdout, "<W> [%s] %s(%d) " fmt "\n", LOG_TAG, __func__, __LINE__, ## args)
#else
#define PRINT_WARN(...)
#endif
#if LOG_LVL >= PRINT_LOG_LVL_ERROR
#define PRINT_ERR(fmt, args...)   fprintf(stderr, "<E> [%s] %s(%d) " fmt "\n", LOG_TAG, __func__, __LINE__, ## args)
#else
#define PRINT_ERR(...)
#endif

#define LOG_PRINT_HEXDUMP(...) log_print_hexdump(__VA_ARGS__)
void log_print_hexdump(const char *, const void*, uint16_t);
