#pragma once
#include "RenderApi.h"

namespace RT
{

	class Renderer
	{
	public:
		static void init()
		{
			renderApi = createRenderApi();
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

		static void beginFrame()
		{
			renderApi->beginFrame();
		}

		static void endFrame()
		{
			renderApi->endFrame();
		}

	private:
		inline static Local<RenderApi> renderApi = nullptr;
	};

}
