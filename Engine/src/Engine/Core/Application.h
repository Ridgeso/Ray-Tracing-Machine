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
		std::function<Local<Frame>()> startupFrameMaker;
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

		void registerAppCallbacks();

	private:
		ApplicationSpecs specs = {};
		
		bool isRunning = true;
		float appFrameDuration = 0.0f;

		Local<Window> window = nullptr;
		Local<Frame> frame = nullptr;

		inline static Application* MainApp = nullptr;
	};
	
	#define RegisterStartupFrame(AppName, StartupFrame)											  \
		RT::ApplicationSpecs CreateApplicationSpec()											  \
		{																						  \
			return RT::ApplicationSpecs{ AppName, [] { return RT::makeLocal<StartupFrame>(); } }; \
		}																						  \

}
