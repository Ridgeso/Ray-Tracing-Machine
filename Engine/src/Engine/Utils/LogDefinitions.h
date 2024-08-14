#pragma once
#include "Engine/Core/Log.h"

#include <glm/glm.hpp>

REGISTER_FMT_FORMAT(glm::vec1, std::string_view, "{}", type.x)
REGISTER_FMT_FORMAT(glm::vec2, std::string_view, "[{}, {}]", type.x, type.y)
REGISTER_FMT_FORMAT(glm::vec3, std::string_view, "[{}, {}, {}]", type.x, type.y, type.z)
REGISTER_FMT_FORMAT(glm::vec4, std::string_view, "[{}, {}, {}, {}]", type.x, type.y, type.z, type.w)

REGISTER_FMT_FORMAT(glm::ivec1, std::string_view, "{}", type.x)
REGISTER_FMT_FORMAT(glm::ivec2, std::string_view, "[{}, {}]", type.x, type.y)
REGISTER_FMT_FORMAT(glm::ivec3, std::string_view, "[{}, {}, {}]", type.x, type.y, type.z)
REGISTER_FMT_FORMAT(glm::ivec4, std::string_view, "[{}, {}, {}, {}]", type.x, type.y, type.z, type.w)

REGISTER_FMT_FORMAT(glm::uvec1, std::string_view, "{}", type.x)
REGISTER_FMT_FORMAT(glm::uvec2, std::string_view, "[{}, {}]", type.x, type.y)
REGISTER_FMT_FORMAT(glm::uvec3, std::string_view, "[{}, {}, {}]", type.x, type.y, type.z)
REGISTER_FMT_FORMAT(glm::uvec4, std::string_view, "[{}, {}, {}, {}]", type.x, type.y, type.z, type.w)
