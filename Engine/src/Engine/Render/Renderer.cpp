#include "Renderer.h"
#include "RenderApi.h"

namespace RT
{

	namespace
	{
		inline static std::unique_ptr<RenderApi> renderApi = nullptr;
	}

	void Renderer::init()
	{
		renderApi = RenderApi::create();
		renderApi->init();
	}

	void Renderer::shutdown()
	{
		renderApi->shutdown();
		renderApi.reset();
	}

	void Renderer::stop()
	{
		renderApi->stop();
	}

	void Renderer::waitForFrameReady()
	{
		renderApi->waitForFrameReady();
	}

	void Renderer::submitUI()
	{
		renderApi->submitUI();
	}

	void Renderer::beginFrame()
	{
		renderApi->beginFrame();
	}

	void Renderer::endFrame()
	{
		renderApi->endFrame();
	}

	bool Renderer::beginCompute()
	{
		return renderApi->beginCompute();
	}

	void Renderer::endCompute()
	{
		renderApi->endCompute();
	}

}
