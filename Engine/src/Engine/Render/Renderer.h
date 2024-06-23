#pragma once
#include <cstdint>
#include <Engine/Core/Base.h>
#include <glm/glm.hpp>

#include "Engine/Render/RenderPass.h"
#include "Engine/Render/Camera.h"
#include "Engine/Render/Scene.h"
#include "Engine/Render/Shader.h"
#include "Engine/Render/Buffer.h"
#include "Engine/Render/Pipeline.h"

namespace RT
{

	struct RenderSpecs
	{
	};

	struct Renderer
	{
		virtual void init(const RenderSpecs& specs) = 0;
		virtual void shutDown() = 0;

		virtual void render(
			const Camera& camera,
			const Shader& shader,
			const Scene& scene,
			const Pipeline& pipeline,
			const Texture& accTexture,
			const Texture& outTexture) = 0;
	};

	Local<Renderer> createRenderer();

}
