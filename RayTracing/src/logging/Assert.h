#pragma once
#include <common/logging/Assert.h>

#include "logging/Log.h"

#define APP_ASSERT(...)	ASSERT("APP", __VA_ARGS__)
