#pragma once
#include "common/logging/Assert.h"

#define RT_ASSERT(...) ASSERT("APP", __VA_ARGS__)
