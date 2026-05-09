#pragma once
#include <functional>
#include <string>

#include "Engine/Window/Window.h"
#include "Engine/Frame/Frame.h"

namespace RT
{

	struct ApplicationSpecs
	{
		std::string name;
		std::function<std::unique_ptr<Frame>()> startupFrameMaker;
	};

	class Application final
	{
		friend void runCore();
	public:

		void run();

		static Application& Get() { return *MainApp; }
		static std::unique_ptr<Window>& getWindow() { return Get().window; }

		float appDuration() { return appFrameDuration; }

	private:
		Application(const ApplicationSpecs& specs);
		~Application();

		void registerAppCallbacks();

	private:
		ApplicationSpecs specs = {};
		
		bool isRunning = true;
		float appFrameDuration = 0.0f;

		std::unique_ptr<Window> window = nullptr;
		std::unique_ptr<Frame> frame = nullptr;

		inline static Application* MainApp = nullptr;
	};
	
	#define RegisterStartupFrame(AppName, StartupFrame)											  \
		RT::ApplicationSpecs CreateApplicationSpec()											  \
		{																						  \
			return RT::ApplicationSpecs{ AppName, [] { return std::make_unique<StartupFrame>(); } }; \
		}																						  \

}
