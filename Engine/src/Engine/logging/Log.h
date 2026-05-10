#pragma once
#include "common/logging/Log.h"

// Engine Logs Macros
#define RT_LOG_CRITICAL(...) ::common::Log::logBase<::common::Log::Level::Critical>("ENG", ::common::FileInfo(), __VA_ARGS__)
#define RT_LOG_ERROR(...)    ::common::Log::logBase<::common::Log::Level::Error>(   "ENG", ::common::FileInfo(), __VA_ARGS__)
#define RT_LOG_WARN(...)     ::common::Log::logBase<::common::Log::Level::Warn>(    "ENG", ::common::FileInfo(), __VA_ARGS__)
#define RT_LOG_INFO(...)     ::common::Log::logBase<::common::Log::Level::Info>(    "ENG", ::common::FileInfo(), __VA_ARGS__)
#define RT_LOG_DEBUG(...)    ::common::Log::logBase<::common::Log::Level::Debug>(   "ENG", ::common::FileInfo(), __VA_ARGS__)
#define RT_LOG_TRACE(...)    ::common::Log::logBase<::common::Log::Level::Trace>(   "ENG", ::common::FileInfo(), __VA_ARGS__)
