#include "Utils.h"
#include <cstdint>

#include <GLFW/glfw3.h>

namespace RT::Glfw
{

	std::vector<const char*> getInstanceExtensions()
	{
		uint32_t glfwExtensionCount = 0u;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		return std::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);
	}

}
