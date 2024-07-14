#pragma once
#include <string>

namespace RT
{

	struct ApplicationSpecs
	{
		std::string name;
		bool isRunning;
	};

	class Application
	{
	public:
		Application(const ApplicationSpecs& specs);
		virtual ~Application();

		void run();

		virtual void layout() = 0;
		virtual void update() = 0;

		static Application& Get() { return *MainApp; }

		float appDuration() { return appFrameDuration; }

	private:
		ApplicationSpecs specs;
		float appFrameDuration;

		static Application* MainApp;
	};

	Application* CreateApplication();

}
