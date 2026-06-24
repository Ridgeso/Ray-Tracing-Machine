#pragma once
#include <memory>
#include <concepts>

namespace RT
{

	struct RenderApi;
	
	class Renderer
	{
	public:
		static void init();

		static void shutdown();

		static void stop();

		static void waitForFrameReady();

		static void submitUI();

		static bool frame(std::invocable auto func)
		{
			if (beginFrame())
			{
				func();
				endFrame();
				return true;
			}
			return false;
		}

		static bool compute(std::invocable auto func)
		{
			if (beginCompute())
			{
				func();
				endCompute();
				return true;
			}
			return false;
		}

	private:
		static void beginFrame();

		static void endFrame();

		static bool beginCompute();
	
		static void endCompute();
	};

}
