#pragma once
#include "common/logging/Log.h"

#define DEBUGBREAK __debugbreak()
#define EXPEND_MACRO(MACRO) MACRO

#define ASSERT_IMPL(LOGGER_TYPE, COND, MSG, ...) { if (!(COND)) { LOG_ERROR(LOGGER_TYPE, "`" #COND "` " MSG, __VA_ARGS__); DEBUGBREAK; } }

#define ASSERT(LOGGER_TYPE, ...) EXPEND_MACRO(ASSERT_IMPL(LOGGER_TYPE, __VA_ARGS__, "", ""))
