#pragma once
#include "RenderApi.h"
#include <memory>

namespace RT
{

	class Renderer
	{
	public:
		static void init()
		{
			renderApi = RenderApi::create();
			renderApi->init();
		}

		static void shutdown()
		{
			renderApi->shutdown();
			renderApi.reset();
		}

		static void stop()
		{
			renderApi->stop();
		}

		static void waitForFrameReady()
		{
			renderApi->waitForFrameReady();
		}

		static void beginFrame()
		{
			renderApi->beginFrame();
		}

		static void endFrame()
		{
			renderApi->endFrame();
		}

		static void beginCompute()
		{
			renderApi->beginCompute();
		}

		static void endCompute()
		{
			renderApi->endCompute();
		}

	private:
		inline static std::unique_ptr<RenderApi> renderApi = nullptr;
	};

}
