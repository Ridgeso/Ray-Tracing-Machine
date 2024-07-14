#include <cstdlib>
#include "Startup.h"

#include "Engine/Core/Application.h"
#include "Engine/Core/Log.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Window/Window.h"

namespace RT
{

	struct CommandLineArgs
	{
		int32_t argc;
		char** argv;
	};

	static void preInitCore(CommandLineArgs args)
	{
		RT::Core::Log::init();
	}

	static void initApp()
	{
		auto winSpecs = WindowSpecs{ "Engine", 1280, 720, false };
		Window::instance()->init(winSpecs);

		Renderer::init();
	}

	static void shutdownApp()
	{
		Renderer::shutdown();
		Window::instance()->shutDown();
	}

	static void runCore()
	{
		initApp();

		auto* application = RT::CreateApplication();
		application->run();
		delete application;

		shutdownApp();
	}

	static void postShutdownCore()
	{
		RT::Core::Log::shutdown();
	}

	int32_t Main(int argc, char* argv[])
	{
		auto args = CommandLineArgs{ argc, argv };
		preInitCore(args);
		runCore();
		postShutdownCore();
		return EXIT_SUCCESS;
	}

}
