#include "Application.h"
#include "Time.h"

#include "Engine/Render/Renderer.h"
#include "Engine/Window/Window.h"

namespace RT
{

	Application* Application::MainApp = nullptr;

	Application::Application(const ApplicationSpecs& specs)
		: specs(specs)
		, appFrameDuration(0)
	{
		MainApp = this;

		Window::instance()->setTitleBar(specs.name);
	}

	Application::~Application()
	{
	}

	void Application::run()
	{
		while (specs.isRunning)
		{
			auto appTimer = Timer{};

			update();

			Window::instance()->beginUI();
			layout();
			Window::instance()->endUI();

			specs.isRunning &= Window::instance()->update();
			specs.isRunning &= Window::instance()->pullEvents();

			appFrameDuration = appTimer.Ellapsed();
		}

		Renderer::stop();
	}

}
