#include <cstdlib>
#include "Startup.h"

#include "common/logging/Log.h"

#include "Engine/Core/Application.h"

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
		common::Log::init();
		#ifndef RT_DEBUG
		common::Log::setLevel(common::Log::Level::Error);
		#endif // RT_DEBUG

		LOG_DEBUG("CORE", "APP CORE CREATED");
	}
	
	static void runCore()
	{
		auto specs = CreateApplicationSpec();

		auto* application = new Application{ specs };
		application->run();
		delete application;
	}

	static void postShutdownCore()
	{
		LOG_DEBUG("CORE", "APP CORE DESTROYED");

		common::Log::shutdown();
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
