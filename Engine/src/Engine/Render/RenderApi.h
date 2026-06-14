#pragma once
#include <memory>

namespace RT
{

	struct RenderApi
	{
	public:
		enum class Api
		{
			None,
			//OpenGL,
			Vulkan
		};

	public:
		static std::unique_ptr<RenderApi> create();

		virtual ~RenderApi() = 0 {}

		virtual void init() = 0;
		virtual void shutdown() = 0;
		virtual void stop() = 0;

		virtual void beginFrame() = 0;
		virtual void endFrame() = 0;

		virtual bool beginCompute() = 0;
		virtual void endCompute() = 0;

		virtual void submitUI() = 0;

		virtual void waitForFrameReady() = 0;

	public:
		static Api api;
	};


}
