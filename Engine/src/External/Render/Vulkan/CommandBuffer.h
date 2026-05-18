#pragma once

#include <array>

#include <vulkan/vulkan.h>

#include "utils/Constants.h"

namespace RT::Vulkan
{

    using CommandBuffers = std::array<VkCommandBuffer, Constants::MAX_FRAMES_IN_FLIGHT>;

}
