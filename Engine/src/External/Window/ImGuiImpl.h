#pragma once

#include <GLFW/glfw3.h>

namespace RT::ImGuiImpl
{

	void initImGui(GLFWwindow* window);
	void shutdown();
	void begin();
	void end();

}

