#include "Window.h"
#include "External/Window/GlfwWindow/GlfwWindow.h"

namespace RT
{

	Local<Window> createWindow()
	{
		return makeLocal<GlfwWindow>();
	}

	Local<Window> Window::window = createWindow();

}
