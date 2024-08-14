#include <cstdlib>
#include "Startup.h"

#include "Engine/Core/Application.h"
#include "Engine/Core/Log.h"

extern RT::ApplicationSpecs CreateApplicationSpec();

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
		#ifndef RT_DEBUG
		RT::Core::Log::setLevel(spdlog::level::err);
		#endif // RT_DEBUG

		RT_LOG_DEBUG("APP CORE CREATED");
	}
	

	static void runCore()
	{
		auto specs = CreateApplicationSpec();

		auto* application = new Application(specs);
		application->run();
		delete application;
	}

	static void postShutdownCore()
	{
		RT_LOG_DEBUG("APP CORE DESTROYED");

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
