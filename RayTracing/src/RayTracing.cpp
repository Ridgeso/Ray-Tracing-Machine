#include <Engine/Engine.h>
#include <Engine/Startup/EntryPoint.h>

#include <Engine/Event/AppEvents.h>

#include <Engine/Render/Camera.h>
#include <Engine/Render/Scene.h>
#include <Engine/Render/Pipeline.h>
#include <Engine/Render/Texture.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

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
		, camera(45.0f, 0.1f, 1.0f)
		, scene{}
	{
		loadScene();

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
		infoUniform.materialsCount = scene.materials.size();
		infoUniform.spheresCount = scene.spheres.size();
		infoUniform.trianglesCount = scene.triangles.size();

		ammountsUniform = RT::Uniform::create(RT::UniformType::Uniform, sizeof(InfoUniform));
		ammountsUniform->setData(&infoUniform, sizeof(InfoUniform));

		cameraUniform = RT::Uniform::create(RT::UniformType::Uniform, sizeof(RT::Camera::Spec));
		cameraUniform->setData(&camera.getSpec(), sizeof(RT::Camera::Spec));

		materialsStorage = RT::Uniform::create(RT::UniformType::Storage, sizeof(RT::Material) * scene.materials.size());
		materialsStorage->setData(scene.materials.data(), sizeof(RT::Material) * scene.materials.size());

		spheresStorage = RT::Uniform::create(RT::UniformType::Storage, scene.spheres.size() > 0 ? sizeof(RT::Sphere) * scene.spheres.size() : 1);
		spheresStorage->setData(scene.spheres.data(), sizeof(RT::Sphere) * scene.spheres.size());
		
		trianglesStorage = RT::Uniform::create(RT::UniformType::Storage, scene.triangles.size() > 0 ? sizeof(RT::Triangle) * scene.triangles.size() : 1);
		trianglesStorage->setData(scene.triangles.data(), sizeof(RT::Triangle) * scene.triangles.size());

		auto pipelineSpec = RT::PipelineSpec{};
		pipelineSpec.shaderPath = assetDir / "shaders" / "RayTracing.shader";
		pipelineSpec.uniformLayouts = RT::UniformLayouts{
			{ .nrOfSets = 1, .layout = { RT::UniformType::Image, RT::UniformType::Image, RT::UniformType::Uniform, RT::UniformType::Uniform } },
			{ .nrOfSets = 1, .layout = { RT::UniformType::Storage, RT::UniformType::Storage, RT::UniformType::Storage } }
		};
		pipelineSpec.attachmentFormats = {};
		pipeline = RT::Pipeline::create(pipelineSpec);

		pipeline->updateSet(0, 0, 0, *accumulationTexture);
		pipeline->updateSet(0, 0, 1, *outTexture);
		pipeline->updateSet(0, 0, 2, *ammountsUniform);
		pipeline->updateSet(0, 0, 3, *cameraUniform);
		pipeline->updateSet(1, 0, 0, *materialsStorage);
		pipeline->updateSet(1, 0, 1, *spheresStorage);
		pipeline->updateSet(1, 0, 2, *trianglesStorage);

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
		{
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::Text("App frame took: %.3fms", RT::Application::Get().appDuration());
			ImGui::Text("GPU time: %.3fms", lastFrameDuration);
			ImGui::Text("CPU time: %.3fms", RT::Application::Get().appDuration() - lastFrameDuration);
			ImGui::Text("Frames: %d", infoUniform.frameIndex);

			infoUniform.frameIndex = accumulation ? infoUniform.frameIndex + 1 : 1;

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
		}
		ImGui::End();

		ImGui::Begin("Camera");
		{
			ImGui::Text("Camera settings");

			bool shouldUpdateCamera = false;
			shouldUpdateCamera |= ImGui::DragFloat("Field of view", &camera.fov, 1.0f, 1.0f, 179.0f);
			shouldUpdateCamera |= ImGui::DragFloat("Blur", &camera.getSpec().blurStrength, 0.05f, 0.0f, std::numeric_limits<float>::max());
			shouldUpdateCamera |= ImGui::DragFloat("Defocus", &camera.getSpec().defocusStrength, 0.05f, 0.0f, std::numeric_limits<float>::max());
			shouldUpdateCamera |= ImGui::DragFloat("Focus distance", &camera.getSpec().focusDistance, 0.05f, 1.0f, std::numeric_limits<float>::max());
			if (shouldUpdateCamera)
			{
				camera.recalculateInvProjection();
				cameraUniform->setData(&camera.getSpec(), sizeof(RT::Camera::Spec));
			}
		}
		ImGui::End();

		ImGui::Begin("Scene");
		{
			ImGui::Text("Scene");

			bool shouldUpdateMaterials = false;
			bool shouldUpdateSpehere = false;
			bool shouldUpdateTriangle = false;

			ImGui::Separator();

			if (ImGui::CollapsingHeader("Materials"))
			{
				if (ImGui::Button("Add Material"))
				{
					scene.materials.emplace_back(RT::Material{ { 0.0f, 0.0f, 0.0f }, 0.0, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f });
					shouldUpdateMaterials = true;
				}

				for (size_t materialId = 1; materialId < scene.materials.size(); materialId++)
				{
					if (!ImGui::TreeNode(fmt::format("Material: {}", materialId).c_str()))
					{
						continue;
					}

					ImGui::PushID((int32_t)materialId);
					auto& material = scene.materials[materialId];

					shouldUpdateMaterials |= ImGui::ColorEdit3("Albedo", glm::value_ptr(material.albedo));
					shouldUpdateMaterials |= ImGui::ColorEdit3("Emission Color", glm::value_ptr(material.emissionColor));
					shouldUpdateMaterials |= ImGui::DragFloat("Roughness", &material.roughness, 0.005f, 0.0f, 1.0f);
					shouldUpdateMaterials |= ImGui::DragFloat("Metalic", &material.metalic, 0.005f, 0.0f, 1.0f);
					shouldUpdateMaterials |= ImGui::DragFloat("Specular Probability", &material.specularProbability, 0.005f, 0.0f, 1.0f);
					shouldUpdateMaterials |= ImGui::DragFloat("Emission Power", &material.emissionPower, 0.005f, 0.0f, std::numeric_limits<float>::max());
					shouldUpdateMaterials |= ImGui::DragFloat("Refraction Index", &material.refractionRatio, 0.005f, 1.0f, 32.0f);

					if (ImGui::Button("Delete Sphere"))
					{
						scene.materials.erase(scene.materials.begin() + materialId);
						for (auto& sphere : scene.spheres)
						{
							if (sphere.materialId == materialId)
							{
								sphere.materialId = 0;
								shouldUpdateSpehere = true;
							}
							else if (sphere.materialId > materialId)
							{
								sphere.materialId--;
								shouldUpdateSpehere = true;
							}
						}
						shouldUpdateMaterials = true;
					}

					ImGui::Separator();
					ImGui::PopID();
					ImGui::TreePop();
				}
			}

			ImGui::Separator();

			if (ImGui::CollapsingHeader("Spheres"))
			{
				if (ImGui::Button("Add Sphere"))
				{
					scene.spheres.emplace_back(RT::Sphere{ { 0.0f, 0.0f, -2.0f }, 1.0f, 1 });
					shouldUpdateSpehere = true;
				}

				for (size_t sphereId = 0u; sphereId < scene.spheres.size(); sphereId++)
				{
					if (!ImGui::TreeNode(fmt::format("Sphere: {}", sphereId).c_str()))
					{
						continue;
					}

					ImGui::PushID((int32_t)sphereId);
					auto& sphere = scene.spheres[sphereId];

					shouldUpdateSpehere |= ImGui::DragFloat3("Position", glm::value_ptr(sphere.position), 0.1f);
					shouldUpdateSpehere |= ImGui::DragFloat("Radius", &sphere.radius, 0.01f, 0.0f, std::numeric_limits<float>::max());
					shouldUpdateSpehere |= ImGui::SliderInt("Material", &sphere.materialId, 1, scene.materials.size() - 1);

					if (ImGui::Button("Delete Sphere"))
					{
						scene.spheres.erase(scene.spheres.begin() + sphereId);
						shouldUpdateSpehere = true;
					}

					ImGui::Separator();
					ImGui::PopID();
					ImGui::TreePop();
				}
			}
			
			ImGui::Separator();

			if (ImGui::CollapsingHeader("Triangles"))
			{
				if (ImGui::Button("Triangle"))
				{
					scene.triangles.emplace_back(RT::Triangle{ { 0.0f, 0.0f, 0.0f }, 0.0f, { 0.0f, 0.0f, 0.0f }, 0.0f, { 0.0f, 0.0f, 0.0f }, 1 });
					shouldUpdateTriangle = true;
				}

				for (size_t triangleId = 0u; triangleId < scene.triangles.size(); triangleId++)
				{
					if (!ImGui::TreeNode(fmt::format("Triangle: {}", triangleId).c_str()))
					{
						continue;
					}

					ImGui::PushID((int32_t)triangleId);
					auto& triangle = scene.triangles[triangleId];

					shouldUpdateTriangle |= ImGui::DragFloat3("Position: A", glm::value_ptr(triangle.A), 0.1f);
					shouldUpdateTriangle |= ImGui::DragFloat3("Position: B", glm::value_ptr(triangle.B), 0.1f);
					shouldUpdateTriangle |= ImGui::DragFloat3("Position: C", glm::value_ptr(triangle.C), 0.1f);
					shouldUpdateTriangle |= ImGui::SliderInt("Material", &triangle.materialId, 1, scene.materials.size() - 1);

					if (ImGui::Button("Delete triangle"))
					{
						scene.triangles.erase(scene.triangles.begin() + triangleId);
						shouldUpdateTriangle = true;
					}

					ImGui::Separator();
					ImGui::PopID();
					ImGui::TreePop();
				}
			}
			
			if (shouldUpdateMaterials)
			{
				if (scene.materials.size() != infoUniform.materialsCount)
				{
					infoUniform.materialsCount = scene.materials.size();
					ammountsUniform->setData(&infoUniform.materialsCount, sizeof(float), offsetof(InfoUniform, materialsCount));

					uint32_t matStorSize = sizeof(RT::Material) * scene.materials.size();
					materialsStorage = RT::Uniform::create(RT::UniformType::Storage, matStorSize > 0 ? matStorSize : 1);

					pipeline->updateSet(1, 0, 0, *materialsStorage);
				}

				materialsStorage->setData(scene.materials.data(), sizeof(RT::Material) * scene.materials.size());
			}
			if (shouldUpdateSpehere)
				{
					if (scene.spheres.size() != infoUniform.spheresCount)
					{
						infoUniform.spheresCount = scene.spheres.size();
						ammountsUniform->setData(&infoUniform.spheresCount, sizeof(float), offsetof(InfoUniform, spheresCount));

						uint32_t sphStorSize = sizeof(RT::Sphere) * scene.spheres.size();
						spheresStorage = RT::Uniform::create(RT::UniformType::Storage, sphStorSize > 0 ? sphStorSize : 1);

						pipeline->updateSet(1, 0, 1, *spheresStorage);
					}

					spheresStorage->setData(scene.spheres.data(), sizeof(RT::Sphere) * scene.spheres.size());
				}
			if (shouldUpdateTriangle)
			{
				if (scene.triangles.size() != infoUniform.trianglesCount)
				{
					infoUniform.trianglesCount = scene.triangles.size();
					ammountsUniform->setData(&infoUniform.trianglesCount, sizeof(float), offsetof(InfoUniform, trianglesCount));

					uint32_t triStorSize = sizeof(RT::Triangle) * scene.triangles.size();
					trianglesStorage = RT::Uniform::create(RT::UniformType::Storage, triStorSize > 0 ? triStorSize : 1);

					pipeline->updateSet(1, 0, 2, *trianglesStorage);
				}

				trianglesStorage->setData(scene.triangles.data(), sizeof(RT::Triangle) * scene.triangles.size());
			}
		}
		ImGui::End();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Viewport");
		{
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
				ImVec2(1, 0));
		}
		ImGui::End();
		ImGui::PopStyleVar();

		static bool demo = true;
		ImGui::ShowDemoWindow(&demo);
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
		const glm::vec3& forward = camera.getDirection();

		auto right = glm::cross(forward, up);
		bool moved = false;

		auto newMousePos = RT::Application::getWindow()->getMousePos();
		auto mouseDelta = (newMousePos - lastMousePos) * mouseSenisity;
		lastMousePos = newMousePos;

		if (RT::Application::getWindow()->isKeyPressed(RT::Keys::Keyboard::W))
		{
			auto step = camera.getPosition() + forward * speed * ts;
			camera.getPosition() = step;
			moved = true;
		}
		if (RT::Application::getWindow()->isKeyPressed(RT::Keys::Keyboard::S))
		{
			auto step = camera.getPosition() - forward * speed * ts;
			camera.getPosition() = step;
			moved = true;
		}

		if (RT::Application::getWindow()->isKeyPressed(RT::Keys::Keyboard::D))
		{
			auto step = camera.getPosition() + right * speed * ts;
			camera.getPosition() = step;
			moved = true;
		}
		if (RT::Application::getWindow()->isKeyPressed(RT::Keys::Keyboard::A))
		{
			auto step = camera.getPosition() - right * speed * ts;
			camera.getPosition() = step;
			moved = true;
		}

		if (RT::Application::getWindow()->isKeyPressed(RT::Keys::Keyboard::Q))
		{
			auto step = camera.getPosition() + up * speed * ts;
			camera.getPosition() = step;
			moved = true;
		}
		if (RT::Application::getWindow()->isKeyPressed(RT::Keys::Keyboard::E))
		{
			auto step = camera.getPosition() - up * speed * ts;
			camera.getPosition() = step;
			moved = true;
		}

		if (RT::Application::getWindow()->isMousePressed(RT::Keys::Mouse::ButtonRight))
		{
			RT::Application::getWindow()->cursorMode(RT::Keys::MouseMod::Disabled);
			if (mouseDelta != glm::vec2(0.0f))
			{
				mouseDelta *= rotationSpeed;
				auto q = glm::normalize(glm::cross(
					glm::angleAxis(-mouseDelta.y, right),
					glm::angleAxis(-mouseDelta.x, up)
				));
				camera.getDirection() = glm::rotate(q, camera.getDirection());
				moved = true;
			}
		}
		else
		{
			RT::Application::getWindow()->cursorMode(RT::Keys::MouseMod::Normal);
		}

		moved |= camera.resizeCamera((int32_t)viewportSize.x, (int32_t)viewportSize.y);

		if (moved)
		{
			camera.recalculateInvView();
			infoUniform.frameIndex = 0;
			cameraUniform->setData(&camera.getSpec(), sizeof(RT::Camera::Spec));
		}
	}

	void registerEvents()
	{
		RT::Event::Event<RT::Event::WindowResize>::registerCallback([this](const auto& event)
		{
			if (event.isMinimized)
			{
				return;
			}

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

	void loadScene()
	{
		//**// SCENE 1 //**//
		//scene.materials.emplace_back(RT::Material{ { 0.0f, 0.0f, 0.0f }, 0.0, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f });
		//scene.materials.emplace_back(RT::Material{ { 1.0f, 1.0f, 1.0f }, 0.0, { 1.0f, 1.0f, 1.0f }, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f });
		//scene.materials.emplace_back(RT::Material{ { 0.0f, 0.0f, 1.0f }, 0.0, { 1.0f, 1.0f, 1.0f }, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f });
		//scene.materials.emplace_back(RT::Material{ { 1.0f, 0.0f, 0.0f }, 0.0, { 1.0f, 1.0f, 1.0f }, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f });

		//scene.spheres.emplace_back(RT::Sphere{ { 0.0f, 0.0f, -10007.0f }, 10000.0f, 1 });
		//scene.spheres.emplace_back(RT::Sphere{ { 0.0f, 0.0f, 10003.0f }, 10000.0f, 1 });
		//scene.spheres.emplace_back(RT::Sphere{ { 0.0f, -10001.0f, -2.0f }, 10000.0f, 1 });
		//scene.spheres.emplace_back(RT::Sphere{ { 0.0f, 10009.0f, -2.0f }, 10000.0f, 1 });
		//scene.spheres.emplace_back(RT::Sphere{ { -10005.0f, 0.0f, -2.0f }, 10000.0f, 2 });
		//scene.spheres.emplace_back(RT::Sphere{ { 10005.0f, 0.0f, -2.0f }, 10000.0f, 3 });

		//scene.materials.emplace_back(RT::Material{ { 1.0f, 1.0f, 1.0f }, 0.0, { 1.0f, 1.0f, 1.0f }, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f });
		//scene.spheres.emplace_back(RT::Sphere{ { 0.0f, 18.8f, -2.0f }, 10.0f, 4 });
		//**// SCENE 1 //**//
		
		//**// SCENE 2 //**//
		//uint32_t seed = 93262352u;

		//scene.materials.emplace_back(RT::Material{ { 0.0f, 0.0f, 0.0f }, 0.0, { 0.0f, 0.0f, 0.0f }, 0.0f,  0.0f, 0.0f, 0.0f, 1.0f });
		//scene.materials.emplace_back(RT::Material{ { 1.0f, 1.0f, 1.0f }, 0.0, { 1.0f, 1.0f, 1.0f }, 0.7f,  0.0f, 0.8f, 0.0f, 1.5f });
		//scene.materials.emplace_back(RT::Material{ { 0.2f, 0.5f, 0.7f }, 0.0, { 0.2f, 0.5f, 0.7f }, 0.05f, 0.0f, 0.3f, 0.0f, 1.0f });
		//scene.materials.emplace_back(RT::Material{ { 0.8f, 0.6f, 0.5f }, 0.0, { 0.8f, 0.6f, 0.5f }, 0.0f,  0.0f, 0.3f, 1.0f, 1.0f });
		//scene.materials.emplace_back(RT::Material{ { 0.4f, 0.3f, 0.8f }, 0.0, { 0.8f, 0.6f, 0.5f }, 0.0f,  0.0f, 0.3f, 0.0f, 1.0f });

		//scene.spheres.emplace_back(RT::Sphere{ { 0.0f, 0.0f, -2.0f }, 1.0f, 1 });
		//scene.spheres.emplace_back(RT::Sphere{ { 0.0f, -2001.0f, -2.0f }, 2000.0f, 2 });
		//scene.spheres.emplace_back(RT::Sphere{ { 2.5f, 0.0f, -2.0f }, 1.0f, 3 });
		//scene.spheres.emplace_back(RT::Sphere{ { -2.5f, 0.0f, -2.0f }, 1.0f, 4 });

		//auto getRandPos = [&seed](float rad) { return FastRandom(seed) * rad - rad / 2; };

		//for (int i = 0; i < 70; i++)
		//{
		//	scene.materials.emplace_back(RT::Material{ });
		//	scene.materials[scene.materials.size() - 1].albedo = { FastRandom(seed), FastRandom(seed), FastRandom(seed) };
		//	scene.materials[scene.materials.size() - 1].emissionColor = { FastRandom(seed), FastRandom(seed), FastRandom(seed) };
		//	scene.materials[scene.materials.size() - 1].roughness = FastRandom(seed) > 0.9 ? 0.f : FastRandom(seed);
		//	scene.materials[scene.materials.size() - 1].metalic = FastRandom(seed) > 0.9 ? FastRandom(seed) : 0.f;
		//	scene.materials[scene.materials.size() - 1].specularProbability= FastRandom(seed) > 0.9 ? FastRandom(seed) : 0.f;
		//	scene.materials[scene.materials.size() - 1].emissionPower = FastRandom(seed) > 0.9 ? FastRandom(seed) : 0.f;
		//	scene.materials[scene.materials.size() - 1].refractionRatio = 1.0f;

		//	scene.spheres.emplace_back(RT::Sphere{ });
		//	scene.spheres[scene.spheres.size() - 1].position = { getRandPos(10.0f), -0.75, getRandPos(10.0f) - 2 };
		//	scene.spheres[scene.spheres.size() - 1].radius = 0.25;
		//	scene.spheres[scene.spheres.size() - 1].materialId = scene.materials.size() - 1;
		//}
		//**// SCENE 2 //**//

		//**// SCENE 3 //**//
		scene.materials.emplace_back(RT::Material{ { 0.0f, 0.0f, 0.0f }, 0.0, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f });

		scene.materials.emplace_back(RT::Material{ { 1.0f, 1.0f, 1.0f }, 0.0, { 1.0f, 1.0f, 1.0f }, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f });
		scene.materials.emplace_back(RT::Material{ { 0.0f, 0.0f, 1.0f }, 0.0, { 1.0f, 1.0f, 1.0f }, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f });
		scene.materials.emplace_back(RT::Material{ { 1.0f, 0.0f, 0.0f }, 0.0, { 1.0f, 1.0f, 1.0f }, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f });
		scene.materials.emplace_back(RT::Material{ { 1.0f, 1.0f, 1.0f }, 0.0, { 1.0f, 1.0f, 1.0f }, 0.0f, 0.0f, 0.0f, 4.0f, 1.0f });

		// bottom
		scene.triangles.emplace_back(RT::Triangle{ { 3.0f, 0.0f, 1.0f }, 0.0, {  3.0f, 0.0f, -5.0f }, 0.0, { -3.0f, 0.0f, -5.0f }, 1 });
		scene.triangles.emplace_back(RT::Triangle{ { 3.0f, 0.0f, 1.0f }, 0.0, { -3.0f, 0.0f, -5.0f }, 0.0, { -3.0f, 0.0f,  1.0f }, 1 });

		// top
		scene.triangles.emplace_back(RT::Triangle{ {  3.0f, 6.0f, -5.0f }, 0.0, { 3.0f, 6.0f, 1.0f }, 0.0, { -3.0f, 6.0f, -5.0f }, 1 });
		scene.triangles.emplace_back(RT::Triangle{ { -3.0f, 6.0f, -5.0f }, 0.0, { 3.0f, 6.0f, 1.0f }, 0.0, { -3.0f, 6.0f,  1.0f }, 1 });

		// back
		scene.triangles.emplace_back(RT::Triangle{ {  3.0f, 0.0f, -5.0f }, 0.0, { 3.0f, 6.0f, -5.0f }, 0.0, { -3.0f, 0.0f, -5.0f }, 1 });
		scene.triangles.emplace_back(RT::Triangle{ { -3.0f, 0.0f, -5.0f }, 0.0, { 3.0f, 6.0f, -5.0f }, 0.0, { -3.0f, 6.0f, -5.0f }, 1 });

		// front
		scene.triangles.emplace_back(RT::Triangle{ { 3.0f, 6.0f, 1.0f }, 0.0, {  3.0f, 0.0f, 1.0f }, 0.0, { -3.0f, 0.0f, 1.0f }, 1 });
		scene.triangles.emplace_back(RT::Triangle{ { 3.0f, 6.0f, 1.0f }, 0.0, { -3.0f, 0.0f, 1.0f }, 0.0, { -3.0f, 6.0f, 1.0f }, 1 });

		// left
		scene.triangles.emplace_back(RT::Triangle{ { 3.0f, 0.0f, -5.0f }, 0.0, { 3.0f, 0.0f, 1.0f }, 0.0, { 3.0f, 6.0f, -5.0f }, 2 });
		scene.triangles.emplace_back(RT::Triangle{ { 3.0f, 0.0f,  1.0f }, 0.0, { 3.0f, 6.0f, 1.0f }, 0.0, { 3.0f, 6.0f, -5.0f }, 2 });

		// right
		scene.triangles.emplace_back(RT::Triangle{ { -3.0f, 0.0f, -5.0f }, 0.0, { -3.0f, 6.0f, -5.0f }, 0.0, { -3.0f, 0.0f, 1.0f }, 3 });
		scene.triangles.emplace_back(RT::Triangle{ { -3.0f, 0.0f,  1.0f }, 0.0, { -3.0f, 6.0f, -5.0f }, 0.0, { -3.0f, 6.0f, 1.0f }, 3 });

		// light
		scene.triangles.emplace_back(RT::Triangle{ {  0.5f, 5.9f, -2.5f }, 0.0, { 0.5f, 5.9f, -1.5f }, 0.0, { -0.5f, 5.9f, -2.5f }, 4 });
		scene.triangles.emplace_back(RT::Triangle{ { -0.5f, 5.9f, -2.5f }, 0.0, { 0.5f, 5.9f, -1.5f }, 0.0, { -0.5f, 5.9f, -1.5f }, 4 });
		//**// SCENE 3 //**//
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
	RT::Local<RT::Uniform> trianglesStorage;

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
		int32_t trianglesCount = 0;
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

	inline static const auto assetDir = std::filesystem::path("assets");
};

RegisterStartupFrame("Ray Tracing", RayTracingClient)
