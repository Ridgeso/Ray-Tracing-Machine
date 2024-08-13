#include <Engine/Engine.h>
#include <Engine/Startup/EntryPoint.h>

#include <Engine/Event/AppEvents.h>

#include <Engine/Render/Camera.h>
#include <Engine/Render/Scene.h>
#include <Engine/Render/Pipeline.h>
#include <Engine/Render/Texture.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <GLFW/glfw3.h>

uint32_t pcg_hash(uint32_t input)
{
	uint32_t state = input * 747796405u + 2891336453u;
	uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return (word >> 22u) ^ word;
}

float FastRandom(uint32_t& seed)
{
	seed = pcg_hash(seed);
	return (float)seed / std::numeric_limits<uint32_t>::max();
}

class RayTracingClient : public RT::Frame
{
public:
	RayTracingClient()
		: viewportSize{}
		, lastFrameDuration{0.0f}
		, lastMousePos{0.0f}
		, lastWinSize{RT::Application::getWindow()->getSize()}
		, camera(45.0f, 0.01f, 100.0f)
		, scene{}
	{
		uint32_t seed = 93262352u;

		scene.materials.emplace_back(RT::Material{ { 0.0f, 0.0f, 0.0f }, 0.0, { 0.0f, 0.0f, 0.0f }, 0.0f,  0.0f, 0.0f, 1.0f });
		scene.materials.emplace_back(RT::Material{ { 1.0f, 1.0f, 1.0f }, 0.0, { 1.0f, 1.0f, 1.0f }, 0.7f,  0.8f, 0.0f, 1.5f });
		scene.materials.emplace_back(RT::Material{ { 0.2f, 0.5f, 0.7f }, 0.0, { 0.2f, 0.5f, 0.7f }, 0.05f, 0.3f, 0.0f, 1.0f });
		scene.materials.emplace_back(RT::Material{ { 0.8f, 0.6f, 0.5f }, 0.0, { 0.8f, 0.6f, 0.5f }, 0.0f,  0.3f, 1.0f, 1.0f });
		scene.materials.emplace_back(RT::Material{ { 0.4f, 0.3f, 0.8f }, 0.0, { 0.8f, 0.6f, 0.5f }, 0.0f,  0.3f, 0.0f, 1.0f });

		scene.spheres.emplace_back(RT::Sphere{ { 0.0f, 0.0f, -2.0f }, 1.0f, 1 });
		scene.spheres.emplace_back(RT::Sphere{ { 0.0f, -2001.0f, -2.0f }, 2000.0f, 2 });
		scene.spheres.emplace_back(RT::Sphere{ { 2.5f, 0.0f, -2.0f }, 1.0f, 3 });
		scene.spheres.emplace_back(RT::Sphere{ { -2.5f, 0.0f, -2.0f }, 1.0f, 4 });

		auto getRandPos = [&seed](float rad) { return FastRandom(seed) * rad - rad / 2; };

		for (int i = 0; i < 70; i++)
		{
			scene.materials.emplace_back(RT::Material{ });
			scene.materials[scene.materials.size() - 1].albedo = { FastRandom(seed), FastRandom(seed), FastRandom(seed) };
			scene.materials[scene.materials.size() - 1].emissionColor = { FastRandom(seed), FastRandom(seed), FastRandom(seed) };
			scene.materials[scene.materials.size() - 1].roughness = FastRandom(seed) > 0.9 ? 0.f : FastRandom(seed);
			scene.materials[scene.materials.size() - 1].emissionPower = FastRandom(seed) > 0.9 ? FastRandom(seed) : 0.f;
			scene.materials[scene.materials.size() - 1].refractionRatio = 1.0f;

			scene.spheres.emplace_back(RT::Sphere{ });
			scene.spheres[scene.spheres.size() - 1].position = { getRandPos(10.0f), -0.75, getRandPos(10.0f) - 2 };
			scene.spheres[scene.spheres.size() - 1].radius = 0.25;
			scene.spheres[scene.spheres.size() - 1].materialId = scene.materials.size() - 1;
		}

		//screenBuff = VertexBuffer::create(sizeof(screenVertices), screenVertices);
		//screenBuff->registerAttributes({ VertexElement::Float2, VertexElement::Float2 });

		//auto renderPassSpec = RenderPassSpec{};
		//renderPassSpec.size = lastWinSize;
		//renderPassSpec.attachmentsFormats = AttachmentFormats{ ImageFormat::RGBA32F, ImageFormat::RGBA32F, ImageFormat::Depth };
		//renderPass = RenderPass::create(renderPassSpec);

		accumulationTexture = RT::Texture::create(lastWinSize, RT::ImageFormat::RGBA32F);
		accumulationTexture->transition(RT::ImageAccess::Write, RT::ImageLayout::General);

		outTexture = RT::Texture::create(lastWinSize, RT::ImageFormat::RGBA8);

		infoUniform.resolution = lastWinSize;
		infoUniform.spheresCount = scene.spheres.size();
		infoUniform.materialsCount = scene.materials.size();

		ammountsUniform = RT::Uniform::create(RT::UniformType::Uniform, sizeof(InfoUniform));
		ammountsUniform->setData(&infoUniform, sizeof(InfoUniform));

		cameraUniform = RT::Uniform::create(RT::UniformType::Uniform, sizeof(RT::Camera::Spec));
		cameraUniform->setData(&camera.GetSpec(), sizeof(RT::Camera::Spec));

		materialsStorage = RT::Uniform::create(RT::UniformType::Storage, sizeof(RT::Material) * scene.materials.size());
		materialsStorage->setData(scene.materials.data(), sizeof(RT::Material) * scene.materials.size());

		spheresStorage = RT::Uniform::create(RT::UniformType::Storage, sizeof(RT::Sphere) * scene.spheres.size());
		spheresStorage->setData(scene.spheres.data(), sizeof(RT::Sphere) * scene.spheres.size());

		auto pipelineSpec = RT::PipelineSpec{};
		pipelineSpec.shaderPath = std::filesystem::path("..") / "Engine" / "assets" / "shaders" / "RayTracing.shader";
		pipelineSpec.uniformLayouts = RT::UniformLayouts{
			{.nrOfSets = 1, .layout = { RT::UniformType::Image, RT::UniformType::Image, RT::UniformType::Uniform, RT::UniformType::Uniform } },
			{.nrOfSets = 1, .layout = { RT::UniformType::Storage, RT::UniformType::Storage } }
		};
		pipelineSpec.attachmentFormats = {};
		pipeline = RT::Pipeline::create(pipelineSpec);

		pipeline->updateSet(0, 0, 0, *accumulationTexture);
		pipeline->updateSet(0, 0, 1, *outTexture);
		pipeline->updateSet(0, 0, 2, *ammountsUniform);
		pipeline->updateSet(0, 0, 3, *cameraUniform);
		pipeline->updateSet(1, 0, 0, *materialsStorage);
		pipeline->updateSet(1, 0, 1, *spheresStorage);

		registerEvents();
	}

	~RayTracingClient()
	{
		//screenBuff.reset();
		//renderPass.reset();

		ammountsUniform.reset();
		cameraUniform.reset();
		materialsStorage.reset();
		spheresStorage.reset();

		accumulationTexture.reset();
		outTexture.reset();
		pipeline.reset();
	}

	void layout() final
	{
		ImGui::Begin("Settings");
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::Text("App frame took: %.3fms", RT::Application::Get().appDuration());
		ImGui::Text("CPU time: %.3fms", lastFrameDuration);
		ImGui::Text("GPU time: %.3fms", RT::Application::Get().appDuration() - lastFrameDuration);
		ImGui::Text("Frames: %d", infoUniform.frameIndex);

		infoUniform.frameIndex++;
		if (!accumulation)
		{
			infoUniform.frameIndex = 1;
		}

		if (ImGui::DragInt("Bounces Limit", (int32_t*)&infoUniform.maxBounces, 1, 1, 15))
		{
			ammountsUniform->setData(&infoUniform.maxBounces, sizeof(uint32_t), offsetof(InfoUniform, maxBounces));
		}
		if (ImGui::DragInt("Precalculated Frames Limit", (int32_t*)&infoUniform.maxFrames, 1, 1, 15))
		{
			ammountsUniform->setData(&infoUniform.maxFrames, sizeof(uint32_t), offsetof(InfoUniform, maxFrames));
		}
		if (ImGui::Button("Reset"))
		{
			infoUniform.frameIndex = 1;
		}
		ammountsUniform->setData(&infoUniform.frameIndex, sizeof(uint32_t), offsetof(InfoUniform, frameIndex));
		ImGui::Checkbox("Accumulate", &accumulation);
		if (ImGui::Checkbox("Draw Environment", &drawEnvironmentTranslator))
		{
			infoUniform.drawEnvironment = drawEnvironmentTranslator;
			ammountsUniform->setData(&infoUniform.drawEnvironment, sizeof(float), offsetof(InfoUniform, drawEnvironment));
		}
		bool shouldUpdateMaterials = false;
		if (ImGui::Button("Add Material"))
		{
			scene.materials.emplace_back(RT::Material{ { 0.0f, 0.0f, 0.0f }, 0.0, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f });
			shouldUpdateMaterials = true;
		}
		bool shouldUpdateSpehere = false;
		if (ImGui::Button("Add Sphere"))
		{
			scene.spheres.emplace_back(RT::Sphere{ { 0.0f, 0.0f, -2.0f }, 1.0f, 0 });
			shouldUpdateSpehere = true;
		}
		ImGui::End();

		ImGui::Begin("Scene");

		ImGui::Text("Materials:");
		for (size_t materialId = 1; materialId < scene.materials.size(); materialId++)
		{
			ImGui::PushID((int32_t)materialId);
			RT::Material& material = scene.materials[materialId];

			shouldUpdateMaterials |= ImGui::ColorEdit3("Albedo", glm::value_ptr(material.albedo));
			shouldUpdateMaterials |= ImGui::ColorEdit3("Emission Color", glm::value_ptr(material.emissionColor));
			shouldUpdateMaterials |= ImGui::DragFloat("Roughness", &material.roughness, 0.005f, 0.0f, 1.0f);
			shouldUpdateMaterials |= ImGui::DragFloat("Metalic", &material.metalic, 0.005f, 0.0f, 1.0f);
			shouldUpdateMaterials |= ImGui::DragFloat("Emission Power", &material.emissionPower, 0.005f, 0.0f, std::numeric_limits<float>::max());
			shouldUpdateMaterials |= ImGui::DragFloat("Refraction Index", &material.refractionRatio, 0.005f, 1.0f, 32.0f);

			ImGui::Separator();
			ImGui::PopID();
		}

		ImGui::Separator();

		ImGui::Text("Spheres:");
		for (size_t sphereId = 0; sphereId < scene.spheres.size(); sphereId++)
		{
			ImGui::PushID((int32_t)sphereId);
			RT::Sphere& sphere = scene.spheres[sphereId];

			shouldUpdateSpehere |= ImGui::DragFloat3("Position", glm::value_ptr(sphere.position), 0.1f);
			shouldUpdateSpehere |= ImGui::DragFloat("Radius", &sphere.radius, 0.01f, 0.0f, std::numeric_limits<float>::max());
			shouldUpdateSpehere |= ImGui::SliderInt("Material", &sphere.materialId, 1, scene.materials.size() - 1);

			ImGui::Separator();
			ImGui::PopID();
		}

		ImGui::End();
		if (shouldUpdateMaterials)
		{
			if (scene.materials.size() == infoUniform.materialsCount)
			{
				materialsStorage->setData(scene.materials.data(), sizeof(RT::Material) * scene.materials.size());
			}
			else
			{
				infoUniform.materialsCount = scene.materials.size();
				ammountsUniform->setData(&infoUniform.materialsCount, sizeof(float), offsetof(InfoUniform, materialsCount));

				materialsStorage = RT::Uniform::create(RT::UniformType::Storage, sizeof(RT::Material) * scene.materials.size());
				materialsStorage->setData(scene.materials.data(), sizeof(RT::Material) * scene.materials.size());
			}
		}
		if (shouldUpdateSpehere)
		{
			if (scene.spheres.size() == infoUniform.spheresCount)
			{
				spheresStorage->setData(scene.spheres.data(), sizeof(RT::Sphere) * scene.spheres.size());
			}
			else
			{
				infoUniform.spheresCount = scene.spheres.size();
				ammountsUniform->setData(&infoUniform.spheresCount, sizeof(float), offsetof(InfoUniform, spheresCount));

				spheresStorage = RT::Uniform::create(RT::UniformType::Storage, sizeof(RT::Sphere) * scene.spheres.size());
				spheresStorage->setData(scene.spheres.data(), sizeof(RT::Sphere) * scene.spheres.size());
			}
		}

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Viewport");

		ImVec2 viewPort = ImGui::GetContentRegionAvail();
		if (viewPort.x != viewportSize.x || viewPort.y != viewportSize.y)
		{
			viewportSize = viewPort;
			infoUniform.frameIndex = 1;
		}

		ImGui::Image(
			outTexture->getTexId(),
			viewportSize,
			ImVec2(0, 1),
			ImVec2(1, 0)
		);

		ImGui::End();
		ImGui::PopStyleVar();

		//static bool demo = true;
		//ImGui::ShowDemoWindow(&demo);
	}

	void update(const float ts) final
	{
		updateView(RT::Application::Get().appDuration() / 1000.0f);

		auto timeit = RT::Timer{};
		RT::Renderer::beginFrame();

		pipeline->bindSet(0, 0);
		pipeline->bindSet(1, 0);

		outTexture->barrier(RT::ImageAccess::Write, RT::ImageLayout::General);

		pipeline->dispatch(outTexture->getSize());

		outTexture->barrier(RT::ImageAccess::Read, RT::ImageLayout::ShaderRead);

		RT::Renderer::endFrame();
		lastFrameDuration = timeit.Ellapsed();
	}

private:
	void updateView(float ts)
	{
		const float speed = 5.0f;
		const float mouseSenisity = 0.003f;
		const float rotationSpeed = 0.3f;
		const glm::vec3 up = glm::vec3(0, 1, 0);
		const glm::vec3& forward = camera.GetDirection();

		glm::vec3 right = glm::cross(forward, up);
		bool moved = false;

		glm::vec2 newMousePos = RT::Application::getWindow()->getMousePos();
		glm::vec2 mouseDelta = (newMousePos - lastMousePos) * mouseSenisity;
		lastMousePos = newMousePos;

		if (RT::Application::getWindow()->isKeyPressed(GLFW_KEY_W))
		{
			glm::vec3 step = camera.GetPosition() + forward * speed * ts;
			camera.SetPosition(step);
			moved = true;
		}
		if (RT::Application::getWindow()->isKeyPressed(GLFW_KEY_S))
		{
			glm::vec3 step = camera.GetPosition() - forward * speed * ts;
			camera.SetPosition(step);
			moved = true;
		}

		if (RT::Application::getWindow()->isKeyPressed(GLFW_KEY_D))
		{
			glm::vec3 step = camera.GetPosition() + right * speed * ts;
			camera.SetPosition(step);
			moved = true;
		}
		if (RT::Application::getWindow()->isKeyPressed(GLFW_KEY_A))
		{
			glm::vec3 step = camera.GetPosition() - right * speed * ts;
			camera.SetPosition(step);
			moved = true;
		}

		if (RT::Application::getWindow()->isKeyPressed(GLFW_KEY_Q))
		{
			glm::vec3 step = camera.GetPosition() + up * speed * ts;
			camera.SetPosition(step);
			moved = true;
		}
		if (RT::Application::getWindow()->isKeyPressed(GLFW_KEY_E))
		{
			glm::vec3 step = camera.GetPosition() - up * speed * ts;
			camera.SetPosition(step);
			moved = true;
		}

		if (RT::Application::getWindow()->isMousePressed(GLFW_MOUSE_BUTTON_RIGHT))
		{
			RT::Application::getWindow()->cursorMode(GLFW_CURSOR_DISABLED);
			if (mouseDelta != glm::vec2(0.0f))
			{
				mouseDelta *= rotationSpeed;
				glm::quat q = glm::normalize(glm::cross(
					glm::angleAxis(-mouseDelta.y, right),
					glm::angleAxis(-mouseDelta.x, up)
				));
				camera.SetDirection(glm::rotate(q, camera.GetDirection()));
				moved = true;
			}
		}
		else
		{
			RT::Application::getWindow()->cursorMode(GLFW_CURSOR_NORMAL);
		}

		moved |= camera.ResizeCamera((int32_t)viewportSize.x, (int32_t)viewportSize.y);

		if (moved)
		{
			camera.RecalculateInvView();
			infoUniform.frameIndex = 0;
			cameraUniform->setData(&camera.GetSpec(), sizeof(RT::Camera::Spec));
		}
	}

	void registerEvents()
	{
		RT::Event::Event<RT::Event::WindowResize>::registerCallback([this](const auto& event)
		{
			lastWinSize = { event.width, event.height };

			infoUniform.resolution = lastWinSize;
			ammountsUniform->setData(&infoUniform.resolution, sizeof(glm::vec2), offsetof(InfoUniform, resolution));

			accumulationTexture = RT::Texture::create(lastWinSize, RT::ImageFormat::RGBA32F);
			accumulationTexture->transition(RT::ImageAccess::Write, RT::ImageLayout::General);

			outTexture = RT::Texture::create(lastWinSize, RT::ImageFormat::RGBA8);

			pipeline->updateSet(0, 0, 0, *accumulationTexture);
			pipeline->updateSet(0, 0, 1, *outTexture);
		});
	}

private:
	ImVec2 viewportSize;
	float lastFrameDuration;
	glm::vec2 lastMousePos;
	glm::ivec2 lastWinSize;

	RT::Camera camera;
	RT::Scene scene;

	RT::Local<RT::Texture> accumulationTexture;
	RT::Local<RT::Texture> outTexture;

	RT::Local<RT::Uniform> cameraUniform;
	RT::Local<RT::Uniform> ammountsUniform;
	RT::Local<RT::Uniform> materialsStorage;
	RT::Local<RT::Uniform> spheresStorage;

	RT::Local<RT::Pipeline> pipeline;

	bool accumulation = false;
	bool drawEnvironmentTranslator = false;

	struct InfoUniform
	{
		float drawEnvironment = (float)false;
		uint32_t maxBounces = 5;
		uint32_t maxFrames = 1;
		uint32_t frameIndex = 1;
		glm::vec2 resolution = {};
		int32_t materialsCount = 0;
		int32_t spheresCount = 0;
		uint32_t renderTexture = 1;
	} infoUniform;

	// TODO: return renderPass and graphics pipeline for post processing
	//Local<VertexBuffer> screenBuff;
	//Share<RenderPass> renderPass;

	//struct Vertices
	//{
	//	float Coords[2];
	//	float TexCoords[2];
	//} static constexpr screenVertices[] = {
	//	{ { -1.0f, -1.0f }, { 0.0f, 0.0f } },
	//	{ {  1.0f, -1.0f }, { 1.0f, 0.0f } },
	//	{ {  1.0f,  1.0f }, { 1.0f, 1.0f } },
	//	{ {  1.0f,  1.0f }, { 1.0f, 1.0f } },
	//	{ { -1.0f,  1.0f }, { 0.0f, 1.0f } },
	//	{ { -1.0f, -1.0f }, { 0.0f, 0.0f } }
	//};
	//static constexpr int32_t screenVerticesCount = sizeof(screenVertices) / sizeof(float);
};

RegisterStartupFrame("Ray Tracing", RayTracingClient)
