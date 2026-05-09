#include "Window.h"
#include "External/Window/GlfwWindow/GlfwWindow.h"

namespace RT
{

	std::unique_ptr<Window> Window::createWindow()
	{
		return std::make_unique<GlfwWindow>();
	}

}
