#pragma once
#include <common/logging/Log.h>

// Application Logs Macros
#define APP_LOG_CRITICAL(...) LOG_CRITICAL("APP", __VA_ARGS__)
#define APP_LOG_ERROR(...)    LOG_ERROR(   "APP", __VA_ARGS__)
#define APP_LOG_WARN(...)     LOG_WARN(    "APP", __VA_ARGS__)
#define APP_LOG_INFO(...)     LOG_INFO(    "APP", __VA_ARGS__)
#define APP_LOG_DEBUG(...)    LOG_DEBUG(   "APP", __VA_ARGS__)
#define APP_LOG_TRACE(...)    LOG_TRACE(   "APP", __VA_ARGS__)
