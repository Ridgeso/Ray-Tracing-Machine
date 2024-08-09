#pragma once
#include <Engine/Core/Base.h>

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
		virtual ~RenderApi() = 0 {}

		virtual void init() = 0;
		virtual void shutdown() = 0;
		virtual void stop() = 0;

		virtual void beginFrame() = 0;
		virtual void endFrame() = 0;

	public:
		static Api api;
	};

	Local<RenderApi> createRenderApi();

}
