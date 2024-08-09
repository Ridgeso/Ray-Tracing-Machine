#include "Window.h"
#include "External/Window/GlfwWindow/GlfwWindow.h"

namespace RT
{

	Local<Window> Window::createWindow()
	{
		return makeLocal<GlfwWindow>();
	}

}
