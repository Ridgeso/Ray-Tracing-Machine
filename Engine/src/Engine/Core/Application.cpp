#include "Engine/Version.h"

#include "Application.h"
#include "Time.h"

#include "Engine/Event/Event.h"
#include "Engine/Event/AppEvents.h"

#include "Engine/Render/Renderer.h"
#include "Engine/Window/Window.h"

namespace RT
{

	Application::Application(const ApplicationSpecs& specs)
		: specs(specs)
		, isRunning(true)
		, appFrameDuration(0)
		, window(Window::createWindow())
	{
		RT_LOG_INFO("APP ** {} ** running [app version __{}__]", specs.name, __RT_VERSION__);
		MainApp = this;

		auto winSpecs = WindowSpecs{ specs.name, 1280, 720, false };
		window->init(winSpecs);

		Renderer::init();

		registerAppCallbacks();

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

			if (window->isMinimize())
			{
				window->update();
				continue;
			}

			window->beginUI();
			frame->layout();
			window->endUI();

			frame->update(appFrameDuration);

			window->update();

			appFrameDuration = appTimer.Ellapsed();
		}

		Renderer::stop();
	}

	void Application::registerAppCallbacks()
	{
		Event::Event<Event::AppClose>::registerCallback([&isRunning = isRunning](const auto& /*unused*/)
		{
			isRunning = false;
			RT_LOG_INFO("Application closing...");
		});
	}

}
