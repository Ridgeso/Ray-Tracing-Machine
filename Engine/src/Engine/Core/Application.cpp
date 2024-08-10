#include "Application.h"
#include "Time.h"

#include "Engine/Render/Renderer.h"
#include "Engine/Window/Window.h"

namespace RT
{

	Application* Application::MainApp = nullptr;

	Application::Application(const ApplicationSpecs& specs)
		: specs(specs)
		, isRunning(true)
		, appFrameDuration(0)
		, window(Window::createWindow())
	{
		MainApp = this;

		auto winSpecs = WindowSpecs{ specs.name, 1280, 720, false };
		window->init(winSpecs);

		Renderer::init();

		frame = specs.startupFrameMaker();
		frame->onInit();
	}

	Application::~Application()
	{
		frame->onShutdown();
		frame.reset();

		Renderer::shutdown();
		window->shutDown();
	}

	void Application::run()
	{
		while (isRunning)
		{
			auto appTimer = Timer{};

			window->beginUI();
			frame->layout();
			window->endUI();

			frame->update(appFrameDuration);

			isRunning &= window->update();
			isRunning &= window->pullEvents();

			appFrameDuration = appTimer.Ellapsed();
		}

		Renderer::stop();
	}

}
