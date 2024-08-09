#pragma once
#include <string>
#include <functional>

#include "Engine/Window/Window.h"
#include "Engine/Frame/Frame.h"

namespace RT
{

	struct ApplicationSpecs
	{
		std::string name;
		std::function<Local<Frame>()> startupFrameMaker = []() { return nullptr; };
	};

	class Application final
	{
		friend void runCore();
	public:

		void run();

		static Application& Get() { return *MainApp; }
		static Local<Window>& getWindow() { return Get().window; }

		float appDuration() { return appFrameDuration; }

	private:
		Application(const ApplicationSpecs& specs);
		~Application();

	private:
		ApplicationSpecs specs;
		
		bool isRunning;
		float appFrameDuration;

		Local<Window> window = nullptr;

		Local<Frame> frame = nullptr;

		static Application* MainApp;
	};
	
	#define RegisterStartupFrame(AppName, StartupFrame)											  \
		RT::ApplicationSpecs CreateApplicationSpec()										  \
		{																						  \
			return RT::ApplicationSpecs{ AppName, [] { return RT::makeLocal<StartupFrame>(); } }; \
		}																						  \

}
