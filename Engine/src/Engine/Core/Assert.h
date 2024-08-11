#pragma once
#include "Engine/Core/Log.h"

#define DEBUGBREAK __debugbreak()

#define ASSERT_IMPL(TYPE, COND, MSG, ...) { if (!(COND)) { ##TYPE##_ERROR("`" #COND "` " MSG, __VA_ARGS__); DEBUGBREAK; } }

#define RT_ASSERT(...) EXPEND_MACRO(ASSERT_IMPL(RT_LOG, __VA_ARGS__, "", ""))
#define ASSERT(...)	   EXPEND_MACRO(ASSERT_IMPL(LOG,    __VA_ARGS__, "", ""))
