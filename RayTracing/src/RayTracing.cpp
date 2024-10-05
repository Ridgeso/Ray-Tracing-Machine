#include <Engine/Engine.h>
#include <Engine/Startup/EntryPoint.h>

#include <array>

#include <Engine/Event/AppEvents.h>

#include <Engine/Render/Camera.h>
#include <Engine/Render/Mesh.h>
#include <Engine/Render/Pipeline.h>
#include <Engine/Render/Texture.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include "SceneWrapper.h"

class RayTracingClient : public RT::Frame
{
public:
	RayTracingClient()
		: viewportSize{}
		, lastFrameDuration{0.0f}
		, lastMousePos{0.0f}
		, lastWinSize{RT::Application::getWindow()->getSize()}
		, selectedScene{3}
		, camera(45.0f, 0.1f, 1.0f)
		, scene{}
		, sceneWrapper{scene}
	{
		//screenBuff = VertexBuffer::create(sizeof(screenVertices), screenVertices);
		//screenBuff->registerAttributes({ VertexElement::Float2, VertexElement::Float2 });

		//auto renderPassSpec = RenderPassSpec{};
		//renderPassSpec.size = lastWinSize;
		//renderPassSpec.attachmentsFormats = AttachmentFormats{ ImageFormat::RGBA32F, ImageFormat::RGBA32F, ImageFormat::Depth };
		//renderPass = RenderPass::create(renderPassSpec);

		loadScene(selectedScene);

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
		bvhStorage.reset();
		trianglesStorage.reset();
		meshWrappersStorage.reset();
		meshInstanceWrappersStorage.reset();

		accumulationTexture.reset();
		outTexture.reset();
		skyMap.reset();
		textures.clear();

		pipeline.reset();
	}

	std::ofstream perfFile;
	bool takeMeasures = false;
	float angle = 0.0f;
	float sampleCnt = 0.0f;
	float sampleSum = 0.0f;
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

			if (ImGui::SliderInt("Bounces Limit", (int32_t*)&infoUniform.maxBounces, 1, 15))
			{
				ammountsUniform->setData(&infoUniform.maxBounces, sizeof(uint32_t), offsetof(InfoUniform, maxBounces));
			}
			if (ImGui::SliderInt("Acumulated Frames", (int32_t*)&infoUniform.maxFrames, 1, 5))
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

			static auto prevSceneLabel = fmt::format("Scene: {}", selectedScene);
			static int32_t selectedMeshId = 0;
			if (ImGui::BeginCombo("Scenes", prevSceneLabel.c_str()))
			{
				for (int32_t i = 1; i <= 5; i++)
				{
					const bool isSceneSelected = i == selectedScene;
					const auto sceneLabel = fmt::format("Scene: {}", i);
					if (ImGui::Selectable(sceneLabel.c_str(), isSceneSelected))
					{
						selectedScene = i;
						prevSceneLabel = sceneLabel;

						scene = RT::Scene{};
						sceneWrapper.~SceneWrapper();
						new (&sceneWrapper) SceneWrapper{scene};
						loadScene(selectedScene);
					}

					if (isSceneSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			if (ImGui::SliderInt("Debug", (int32_t*)&infoUniform.debug, 0, 400))
			{
				ammountsUniform->setData(&infoUniform.debug, sizeof(uint32_t), offsetof(InfoUniform, debug));
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
			if (ImGui::Button("Measure Performance"))
			{
				if (perfFile.is_open())
				{
					perfFile.close();
				}
				perfFile.open("Measure.txt");
				takeMeasures = true;
				angle = 0.0f;
			}
		}
		ImGui::End();

		ImGui::Begin("Scene");
		{
			ImGui::Text("Scene");

			bool shouldUpdateMaterials = false;
			bool shouldUpdateSpeheres = false;
			bool shouldUpdateMeshes = false;
			bool shouldUpdateObjects = false;

			ImGui::Separator();

			if (ImGui::CollapsingHeader("Materials"))
			{
				if (ImGui::Button("Add Material"))
				{
					scene.materials.emplace_back(RT::Material{ { 0.0f, 0.0f, 0.0f }, 0.0, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f, 1.0f, -1 });
					shouldUpdateMaterials = true;
				}

				for (size_t materialId = 0; materialId < scene.materials.size(); materialId++)
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
					shouldUpdateMaterials |= ImGui::DragFloat("Emission Power", &material.emissionPower, 0.005f, 0.0f, std::numeric_limits<float>::max());
					shouldUpdateMaterials |= ImGui::DragFloat("Refraction Index", &material.refractionRatio, 0.005f, 1.0f, 32.0f);
					shouldUpdateMaterials |= ImGui::SliderInt("Texture Id", &material.textureId, -1, textures.size() - 1);
					
					if (ImGui::Button("Delete Material"))
					{
						scene.materials.erase(scene.materials.begin() + materialId);
						for (auto& sphere : sceneWrapper.spheres)
						{
							if (sphere.materialId == materialId)
							{
								sphere.materialId = 0;
								shouldUpdateSpeheres = true;
							}
							else if (sphere.materialId > materialId)
							{
								sphere.materialId--;
								shouldUpdateSpeheres = true;
							}
						}
						shouldUpdateMaterials = true;

						// TODO: adjust when materialId will be moved to 'objects'
						for (auto& object : scene.objects)
						{
							if (object.materialId == materialId)
							{
								object.materialId = 0;
								shouldUpdateObjects = true;
							}
							else if (object.materialId > materialId)
							{
								object.materialId--;
								shouldUpdateObjects = true;
							}
						}
						for (auto& object : sceneWrapper.meshInstanceWrappers)
						{
							if (object.materialId == materialId)
							{
								object.materialId = 0;
								shouldUpdateObjects = true;
							}
							else if (object.materialId > materialId)
							{
								object.materialId--;
								shouldUpdateObjects = true;
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
					sceneWrapper.spheres.emplace_back(Sphere{ { 0.0f, 0.0f, -2.0f }, 1.0f, 0 });
					shouldUpdateSpeheres = true;
				}

				for (size_t sphereId = 0u; sphereId < sceneWrapper.spheres.size(); sphereId++)
				{
					if (!ImGui::TreeNode(fmt::format("Sphere: {}", sphereId).c_str()))
					{
						continue;
					}

					ImGui::PushID((int32_t)sphereId);
					auto& sphere = sceneWrapper.spheres[sphereId];

					shouldUpdateSpeheres |= ImGui::DragFloat3("Position", glm::value_ptr(sphere.position), 0.1f);
					shouldUpdateSpeheres |= ImGui::DragFloat("Radius", &sphere.radius, 0.01f, 0.0f, std::numeric_limits<float>::max());
					shouldUpdateSpeheres |= ImGui::SliderInt("Material", &sphere.materialId, 0, scene.materials.size() - 1);

					if (ImGui::Button("Delete Sphere"))
					{
						sceneWrapper.spheres.erase(sceneWrapper.spheres.begin() + sphereId);
						shouldUpdateSpeheres = true;
					}

					ImGui::Separator();
					ImGui::PopID();
					ImGui::TreePop();
				}
			}
			
			ImGui::Separator();

			if (ImGui::CollapsingHeader("Objects"))
			{
				{
					constexpr uint32_t maxPathLength = 260u;
					static auto newMeshPathBuff = std::array<char, maxPathLength>{};
					ImGui::InputText("Mesh path", newMeshPathBuff.data(), maxPathLength);

					if (ImGui::Button("Add Mesh"))
					{
						const auto newMeshPath = std::filesystem::path(newMeshPathBuff.data());
						const bool doesMeshExists = std::filesystem::exists(newMeshPath);
						if (doesMeshExists)
						{
							auto& newMesh = scene.meshes.emplace_back();
							newMesh.load(newMeshPath);
							sceneWrapper.addMesh(newMesh);
							shouldUpdateMeshes = true;
						}
					}
				}

				{
					static auto previewLabel = std::string{ "Choose mesh" };
					if (ImGui::Button("Add Instance"))
					{
						ImGui::OpenPopup("Add Instance");
						previewLabel = std::string{ "Choose mesh" };
					}

					const auto popupCenter = ImGui::GetMainViewport()->GetCenter();
					ImGui::SetNextWindowPos(popupCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
					if (ImGui::BeginPopupModal("Add Instance", NULL, ImGuiWindowFlags_AlwaysAutoResize))
					{
						static int32_t selectedMeshId = 0;
						if (ImGui::BeginCombo("Mesh", previewLabel.c_str()))
						{
							for (int32_t meshId = 0; meshId < scene.meshes.size(); meshId++)
							{
								const bool isMeshSelected = meshId == selectedMeshId;
								const auto meshName = fmt::format("Mesh: {}", meshId);
								if (ImGui::Selectable(meshName.c_str(), isMeshSelected))
								{
									selectedMeshId = meshId;
									previewLabel = meshName;
								}

								if (isMeshSelected)
								{
									ImGui::SetItemDefaultFocus();
								}
							}
							ImGui::EndCombo();
						}

						if (ImGui::Button("Add", ImVec2(60, 0)))
						{
							auto& newObject = scene.objects.emplace_back(selectedMeshId);
							sceneWrapper.addMeshInstance(newObject);
							shouldUpdateObjects = true;
							ImGui::CloseCurrentPopup();
						}
						ImGui::SetItemDefaultFocus();
						ImGui::SameLine();
						if (ImGui::Button("Cancel", ImVec2(60, 0)))
						{
							ImGui::CloseCurrentPopup();
						}
						ImGui::EndPopup();
					}
				}

				for (size_t objectId = 0u; objectId < scene.objects.size(); objectId++)
				{
					if (!ImGui::TreeNode(fmt::format("Mesh: {}", objectId).c_str()))
					{
						continue;
					}
					
					ImGui::PushID((int32_t)objectId);
					auto& object = scene.objects[objectId];

					shouldUpdateObjects |= ImGui::DragFloat3("Position", glm::value_ptr(object.position), 0.1f);
					shouldUpdateObjects |= ImGui::DragFloat3("Scale", glm::value_ptr(object.scale), 0.1f);
					shouldUpdateObjects |= ImGui::DragFloat3("Rotation", glm::value_ptr(object.rotation), 0.1f, -360.0f, 360.0f);
					shouldUpdateObjects |= ImGui::SliderInt("Material", &object.materialId, 0, scene.materials.size() - 1);
					shouldUpdateObjects |= ImGui::SliderInt("Mesh", &object.meshId, 0, scene.meshes.size() - 1);

					if (shouldUpdateObjects)
					{
						sceneWrapper.meshInstanceWrappers[objectId].worldToLocalMatrix = object.getInvModelMatrix();
						sceneWrapper.meshInstanceWrappers[objectId].materialId = object.materialId;
						sceneWrapper.meshInstanceWrappers[objectId].meshId = object.meshId;
					}

					if (ImGui::Button("Delete Object"))
					{
						scene.objects.erase(scene.objects.begin() + objectId);
						sceneWrapper.removeInstanceWrapper(objectId);
						shouldUpdateObjects = true;
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
					ammountsUniform->setData(&infoUniform.materialsCount, sizeof(int32_t), offsetof(InfoUniform, materialsCount));

					const uint32_t matStorSize = sizeof(RT::Material) * scene.materials.size();
					materialsStorage = RT::Uniform::create(RT::UniformType::Storage, matStorSize > 0 ? matStorSize : 1);

					pipeline->updateSet(1, 0, 0, *materialsStorage);
				}

				materialsStorage->setData(scene.materials.data(), sizeof(RT::Material) * scene.materials.size());
			}
			if (shouldUpdateSpeheres)
				{
					if (sceneWrapper.spheres.size() != infoUniform.spheresCount)
					{
						infoUniform.spheresCount = sceneWrapper.spheres.size();
						ammountsUniform->setData(&infoUniform.spheresCount, sizeof(int32_t), offsetof(InfoUniform, spheresCount));

						const uint32_t sphStorSize = sizeof(Sphere) * sceneWrapper.spheres.size();
						spheresStorage = RT::Uniform::create(RT::UniformType::Storage, sphStorSize > 0 ? sphStorSize : 1);

						pipeline->updateSet(1, 0, 1, *spheresStorage);
					}

					spheresStorage->setData(sceneWrapper.spheres.data(), sizeof(Sphere) * sceneWrapper.spheres.size());
				}
			if (shouldUpdateMeshes)
			{
				const uint32_t bvhStorSize = sizeof(BoundingBox) * sceneWrapper.boundingBoxes.size();
				bvhStorage = RT::Uniform::create(RT::UniformType::Storage, bvhStorSize > 0 ? bvhStorSize : 1);
				pipeline->updateSet(1, 0, 2, *bvhStorage);
				bvhStorage->setData(sceneWrapper.boundingBoxes.data(), bvhStorSize);

				const uint32_t triStorSize = sizeof(RT::Triangle) * sceneWrapper.triangles.size();
				trianglesStorage = RT::Uniform::create(RT::UniformType::Storage, triStorSize > 0 ? triStorSize : 1);
				pipeline->updateSet(1, 0, 3, *trianglesStorage);
				trianglesStorage->setData(sceneWrapper.triangles.data(), triStorSize);

				const uint32_t objStorSize = sizeof(MeshWrapper) * sceneWrapper.meshWrappers.size();
				meshWrappersStorage = RT::Uniform::create(RT::UniformType::Storage, objStorSize > 0 ? objStorSize : 1);
				pipeline->updateSet(1, 0, 4, *meshWrappersStorage);
				meshWrappersStorage->setData(sceneWrapper.meshWrappers.data(), objStorSize);
			}
			if (shouldUpdateObjects)
			{
				if (sceneWrapper.meshInstanceWrappers.size() != infoUniform.objectsCount)
				{
					infoUniform.objectsCount = sceneWrapper.meshInstanceWrappers.size();
					ammountsUniform->setData(&infoUniform.objectsCount, sizeof(int32_t), offsetof(InfoUniform, objectsCount));

					const uint32_t objStorSize = sizeof(MeshInstanceWrapper) * sceneWrapper.meshInstanceWrappers.size();
					meshInstanceWrappersStorage = RT::Uniform::create(RT::UniformType::Storage, objStorSize > 0 ? objStorSize : 1);

					pipeline->updateSet(1, 0, 5, *meshInstanceWrappersStorage);
				}
				 
				meshInstanceWrappersStorage->setData(sceneWrapper.meshInstanceWrappers.data(), sizeof(MeshInstanceWrapper) * sceneWrapper.meshInstanceWrappers.size());
			}
		}
		ImGui::End();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
		ImGui::Begin("Viewport");
		{
			auto viewPort = ImGui::GetContentRegionAvail();
			if (viewPort.x != viewportSize.x || viewPort.y != viewportSize.y)
			{
				viewportSize = viewPort;
				infoUniform.frameIndex = 1;

				infoUniform.resolution = glm::vec2(viewportSize.x, viewportSize.y);
				ammountsUniform->setData(&infoUniform.resolution, sizeof(glm::vec2), offsetof(InfoUniform, resolution));

				accumulationTexture = RT::Texture::create(infoUniform.resolution, RT::Texture::Format::RGBA32F);
				accumulationTexture->transition(RT::Texture::Access::Write, RT::Texture::Layout::General);

				outTexture = RT::Texture::create(infoUniform.resolution, RT::Texture::Format::RGBA8);

				pipeline->updateSet(0, 0, 0, *accumulationTexture);
				pipeline->updateSet(0, 0, 1, *outTexture);
			}

			ImGui::Image(
				outTexture->getTexId(),
				viewportSize,
				ImVec2(0, 1),
				ImVec2(1, 0)
			);
		}
		ImGui::End();
		ImGui::PopStyleVar();

		// static bool demo = true;
		// ImGui::ShowDemoWindow(&demo);
	}

	void update(const float ts) final
	{
		updateView(RT::Application::Get().appDuration() / 1000.0f);

		auto timeit = RT::Timer{};
		RT::Renderer::beginFrame();

		pipeline->bindSet(0, 0);
		pipeline->bindSet(1, 0);

		outTexture->barrier(RT::Texture::Access::Write, RT::Texture::Layout::General);

		pipeline->dispatch(outTexture->getSize());

		outTexture->barrier(RT::Texture::Access::Read, RT::Texture::Layout::ShaderRead);

		RT::Renderer::endFrame();
		lastFrameDuration = timeit.Ellapsed();
	}

private:
	void updateView(float ts)
	{
		const float speed = 1.0f;
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

		if (takeMeasures)
		{
			sampleCnt += 1.0f;
			sampleSum += RT::Application::Get().appDuration();

			if (sampleCnt >= 3.0f)
			{
				perfFile << angle << " " << (sampleSum / sampleCnt) << std::endl;
				sampleCnt = 0.0f;
				sampleSum = 0.0f;
				angle += 1.0f;

				moved = true;
				camera.getPosition() = 2.0f * glm::vec3{ glm::cos(glm::radians(angle)), 0.f, glm::sin(glm::radians(angle)) };
				camera.getDirection() = -camera.getPosition();

				cameraUniform->setData(&camera.getSpec(), sizeof(RT::Camera::Spec));
			}

			if (angle >= 360.0f)
			{
				takeMeasures = false;
			}
		}

		if (moved)
		{
			camera.recalculateInvView();
			infoUniform.frameIndex = 0;
			cameraUniform->setData(&camera.getSpec(), sizeof(RT::Camera::Spec));
		}
	}

	void registerEvents()
	{
	}

	void loadScene(const int32_t sceneNr)
	{
		switch (sceneNr)
		{
			case 1:
			{
				//**// SCENE 1 //**//
				scene.materials.emplace_back(RT::Material{ { 1.0f, 1.0f, 1.0f }, 0.0, { 1.0f, 1.0f, 1.0f }, 0.0f, 0.0f, 0.0f, 1.0f, -1 });
				scene.materials.emplace_back(RT::Material{ { 0.0f, 0.0f, 1.0f }, 0.0, { 1.0f, 1.0f, 1.0f }, 0.0f, 0.0f, 0.0f, 1.0f, -1 });
				scene.materials.emplace_back(RT::Material{ { 1.0f, 0.0f, 0.0f }, 0.0, { 1.0f, 1.0f, 1.0f }, 0.0f, 0.0f, 0.0f, 1.0f, -1 });

				sceneWrapper.spheres.emplace_back(Sphere{ { 0.0f, 0.0f, -10007.0f }, 10000.0f, 0 });
				sceneWrapper.spheres.emplace_back(Sphere{ { 0.0f, 0.0f, 10003.0f }, 10000.0f, 0 });
				sceneWrapper.spheres.emplace_back(Sphere{ { 0.0f, -10001.0f, -2.0f }, 10000.0f, 0 });
				sceneWrapper.spheres.emplace_back(Sphere{ { 0.0f, 10009.0f, -2.0f }, 10000.0f, 0 });
				sceneWrapper.spheres.emplace_back(Sphere{ { -10005.0f, 0.0f, -2.0f }, 10000.0f, 1 });
				sceneWrapper.spheres.emplace_back(Sphere{ { 10005.0f, 0.0f, -2.0f }, 10000.0f, 2 });

				scene.materials.emplace_back(RT::Material{ { 1.0f, 1.0f, 1.0f }, 0.0, { 1.0f, 1.0f, 1.0f }, 0.0f, 0.0f, 1.0f, 1.0f, -1 });
				sceneWrapper.spheres.emplace_back(Sphere{ { 0.0f, 18.8f, -2.0f }, 10.0f, 3 });
				//**// SCENE 1 //**//
			
				break;
			}

			case 2:
			{
				//**// SCENE 2 //**//
				textures.emplace_back(RT::Texture::create(assetDir / "textures" / "templategrid_albedo.png"));
				textures[0]->transition(RT::Texture::Access::Read, RT::Texture::Layout::General);

				scene.materials.emplace_back(RT::Material{ { 1.0f, 1.0f, 1.0f }, 0.0, { 1.0f, 1.0f, 1.0f }, 0.7f, 0.0f, 0.0f, 1.5f, -1 });
				scene.materials.emplace_back(RT::Material{ { 0.2f, 0.5f, 0.7f }, 0.0, { 0.2f, 0.5f, 0.7f }, 0.0f, 0.0f, 0.0f, 1.0f,  0 });
				scene.materials.emplace_back(RT::Material{ { 0.8f, 0.6f, 0.5f }, 0.0, { 0.8f, 0.6f, 0.5f }, 0.0f, 0.0f, 1.0f, 1.0f, -1 });
				scene.materials.emplace_back(RT::Material{ { 0.4f, 0.3f, 0.8f }, 0.0, { 0.8f, 0.6f, 0.5f }, 0.0f, 0.0f, 0.0f, 1.0f, -1 });

				sceneWrapper.spheres.emplace_back(Sphere{ { 0.0f, 0.0f, -2.0f }, 1.0f, 0 });
				
				auto triBuffer = std::vector<RT::Triangle>{
					RT::Triangle{
						{ -50.0f, -1.0f, -50.0f }, { -50.0f, -1.0f,  50.0f }, {  50.0f, -1.0f, -50.0f },
						{  0.0,  0.0 }, {  0.0, 10.0 }, { 10.0,  0.0 }},
					RT::Triangle{
						{  50.0f, -1.0f,  50.0f }, {  50.0f, -1.0f, -50.0f }, { -50.0f, -1.0f,  50.0f },
						{ 10.0, 10.0 }, { 10.0,  0.0 }, {  0.0, 10.0 }}};
				scene.meshes.emplace_back(triBuffer);
				
				scene.objects.emplace_back(0);
				scene.objects[0].materialId = 1;

				sceneWrapper.spheres.emplace_back(Sphere{ {  2.5f, 0.0f, -2.0f }, 1.0f, 2 });
				sceneWrapper.spheres.emplace_back(Sphere{ { -2.5f, 0.0f, -2.0f }, 1.0f, 3 });

				//auto pcg_hash = [](uint32_t input) -> uint32_t
				//{
				//	uint32_t state = input * 747796405u + 2891336453u;
				//	uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
				//	return (word >> 22u) ^ word;
				//};

				//auto FastRandom = [&pcg_hash](uint32_t& seed) -> float
				//{
				//	seed = pcg_hash(seed);
				//	return (float)seed / std::numeric_limits<uint32_t>::max();
				//}

				//uint32_t seed = 93262352u;
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
				//	scene.materials[scene.materials.size() - 1].textureId = -1;

				//	scene.spheres.emplace_back(RT::Sphere{ });
				//	scene.spheres[scene.spheres.size() - 1].position = { getRandPos(10.0f), -0.75, getRandPos(10.0f) - 2 };
				//	scene.spheres[scene.spheres.size() - 1].radius = 0.25;
				//	scene.spheres[scene.spheres.size() - 1].materialId = scene.materials.size() - 1;
				//}
				//**// SCENE 2 //**//
			
				break;
			}

			case 3:
			{
				//**// SCENE 3 //**//
				textures.emplace_back(RT::Texture::create(assetDir / "textures" / "checkered.jpg"));
				textures[0]->transition(RT::Texture::Access::Read, RT::Texture::Layout::General);

				scene.materials.emplace_back(RT::Material{ { 1.0f, 1.0f, 1.0f }, 0.0, { 1.0f, 1.0f, 1.0f }, 0.0f, 0.0f, 0.0f, 1.0f, -1 });
				scene.materials.emplace_back(RT::Material{ { 1.0f, 1.0f, 1.0f }, 0.0, { 1.0f, 1.0f, 1.0f }, 0.0f, 0.0f, 0.0f, 1.0f,  0 });
				scene.materials.emplace_back(RT::Material{ { 1.0f, 1.0f, 1.0f }, 0.0, { 1.0f, 1.0f, 1.0f }, 0.0f, 0.0f, 0.0f, 1.0f, -1 });
				scene.materials.emplace_back(RT::Material{ { 1.0f, 0.0f, 0.0f }, 0.0, { 1.0f, 1.0f, 1.0f }, 0.0f, 0.0f, 0.0f, 1.0f, -1 });
				scene.materials.emplace_back(RT::Material{ { 0.0f, 1.0f, 0.0f }, 0.0, { 1.0f, 1.0f, 1.0f }, 0.0f, 0.0f, 0.0f, 1.0f, -1 });
				scene.materials.emplace_back(RT::Material{ { 1.0f, 1.0f, 1.0f }, 0.0, { 1.0f, 1.0f, 1.0f }, 0.0f, 0.0f, 8.0f, 1.0f, -1 });

				scene.meshes.emplace_back().load(assetDir / "models" / "tinyStanfordDragon.glb");
				auto& dragon = scene.objects.emplace_back(0);
				dragon.position = glm::vec3{ 0.0f, 1.4f, -2.0f };
				dragon.scale = glm::vec3{ 5.0f };
				dragon.rotation = glm::vec3{ 0.0f, 128.0, 0.0f };
				dragon.materialId = 0;

				auto triBottom = std::vector<RT::Triangle>{
					RT::Triangle{
						{  3.0f, 0.0f,  1.0f }, {  3.0f, 0.0f, -5.0f }, { -3.0f, 0.0f, 1.0f },
						{ 0.0, 0.0 }, { 0.0, 1.0 }, { 1.0, 0.0 }},
					RT::Triangle{
						{ -3.0f, 0.0f, -5.0f }, { -3.0f, 0.0f,  1.0f }, {  3.0f, 0.0f, -5.0f },
						{ 1.0, 1.0 }, { 1.0, 0.0 }, { 0.0, 1.0 }}};
				auto triTop = std::vector<RT::Triangle>{
					RT::Triangle{
						{  3.0f, 6.0f, -5.0f }, {  3.0f, 6.0f,  1.0f }, { -3.0f, 6.0f, -5.0f },
						{ 0.0, 0.0 }, { 0.0, 0.0 }, { 0.0, 0.0 }},
					RT::Triangle{
						{ -3.0f, 6.0f, -5.0f }, {  3.0f, 6.0f,  1.0f }, { -3.0f, 6.0f,  1.0f },
						{ 0.0, 0.0 }, { 0.0, 0.0 }, { 0.0, 0.0 }}};
				auto triBack = std::vector<RT::Triangle>{
					RT::Triangle{
						{  3.0f, 0.0f, -5.0f }, {  3.0f, 6.0f, -5.0f }, { -3.0f, 0.0f, -5.0f },
						{ 0.0, 0.0 }, { 0.0, 0.0 }, { 0.0, 0.0 }},
					RT::Triangle{
						{ -3.0f, 0.0f, -5.0f }, {  3.0f, 6.0f, -5.0f }, { -3.0f, 6.0f, -5.0f },
						{ 0.0, 0.0 }, { 0.0, 0.0 }, { 0.0, 0.0 }} };
				auto triFront = std::vector<RT::Triangle>{
					RT::Triangle{
						{  3.0f, 6.0f, 1.0f }, {  3.0f, 0.0f, 1.0f }, { -3.0f, 0.0f, 1.0f },
						{ 0.0, 0.0 }, { 0.0, 0.0 }, { 0.0, 0.0 }},
					RT::Triangle{
						{  3.0f, 6.0f, 1.0f }, { -3.0f, 0.0f, 1.0f }, { -3.0f, 6.0f, 1.0f },
						{ 0.0, 0.0 }, { 0.0, 0.0 }, { 0.0, 0.0 }} };
				auto triLeft = std::vector<RT::Triangle>{
					RT::Triangle{
						{ 3.0f, 0.0f, -5.0f }, { 3.0f, 0.0f,  1.0f }, { 3.0f, 6.0f, -5.0f },
						{ 0.0, 0.0 }, { 0.0, 0.0 }, { 0.0, 0.0 }
					},
					RT::Triangle{
						{ 3.0f, 0.0f,  1.0f }, { 3.0f, 6.0f,  1.0f }, { 3.0f, 6.0f, -5.0f },
						{ 0.0, 0.0 }, { 0.0, 0.0 }, { 0.0, 0.0 }} };
				auto triRight = std::vector<RT::Triangle>{
					RT::Triangle{
						{ -3.0f, 0.0f, -5.0f }, { -3.0f, 6.0f, -5.0f }, { -3.0f, 0.0f,  1.0f },
						{ 0.0, 0.0 }, { 0.0, 0.0 }, { 0.0, 0.0 }},
					RT::Triangle{
						{ -3.0f, 0.0f,  1.0f }, { -3.0f, 6.0f, -5.0f }, { -3.0f, 6.0f,  1.0f },
						{ 0.0, 0.0 }, { 0.0, 0.0 }, { 0.0, 0.0 }} };

				auto triLight = std::vector<RT::Triangle>{
					RT::Triangle{
						{  1.0f, 5.9f, -3.0f }, {  1.0f, 5.9f, -1.0f }, { -1.0f, 5.9f, -3.0f },
						{ 0.0, 0.0 }, { 0.0, 0.0 }, { 0.0, 0.0 }},
					RT::Triangle{
						{ -1.0f, 5.9f, -3.0f }, {  1.0f, 5.9f, -1.0f }, { -1.0f, 5.9f, -1.0f },
						{ 0.0, 0.0 }, { 0.0, 0.0 }, { 0.0, 0.0 }}};

				scene.meshes.emplace_back(triBottom);
				scene.meshes.emplace_back(triTop);
				scene.meshes.emplace_back(triBack);
				scene.meshes.emplace_back(triFront);
				scene.meshes.emplace_back(triLeft);
				scene.meshes.emplace_back(triRight);
				scene.meshes.emplace_back(triLight);

				scene.objects.emplace_back(1).materialId = 1;
				scene.objects.emplace_back(2).materialId = 2;
				scene.objects.emplace_back(3).materialId = 2;
				scene.objects.emplace_back(4).materialId = 2;
				scene.objects.emplace_back(5).materialId = 3;
				scene.objects.emplace_back(6).materialId = 4;
				scene.objects.emplace_back(7).materialId = 5;
				//**// SCENE 3 //**//

				break;
			}
			case 4:
			{
				//**// SCENE 4 //**//
				// Dev platform
				scene.materials.emplace_back(RT::Material{ { 1.0f, 1.0f, 1.0f }, 0.0, { 1.0f, 1.0f, 1.0f }, 0.0f, 0.0f, 0.0f, 1.0f, -1 });

				scene.meshes.emplace_back().load(assetDir / "models" / "tinyStanfordDragon.glb");
				scene.objects.emplace_back(0);
				//**// SCENE 4 //**//

				break;
			}
		}

		sceneWrapper.build();
		constructScene();
	}

	void constructScene()
	{
		accumulationTexture = RT::Texture::create(lastWinSize, RT::Texture::Format::RGBA32F);
		accumulationTexture->transition(RT::Texture::Access::Write, RT::Texture::Layout::General);

		outTexture = RT::Texture::create(lastWinSize, RT::Texture::Format::RGBA8);

		skyMap = RT::Texture::create(assetDir / "skyMaps" / "evening_road_01_puresky_1k.hdr", RT::Texture::Filter::Linear, RT::Texture::Mode::ClampToEdge);
		skyMap->transition(RT::Texture::Access::Read, RT::Texture::Layout::General);

		infoUniform.resolution = lastWinSize;
		infoUniform.materialsCount = scene.materials.size();
		infoUniform.spheresCount = sceneWrapper.spheres.size();
		infoUniform.objectsCount = sceneWrapper.meshInstanceWrappers.size();
		infoUniform.texturesCount = textures.size();

		ammountsUniform = RT::Uniform::create(RT::UniformType::Uniform, sizeof(InfoUniform));
		ammountsUniform->setData(&infoUniform, sizeof(InfoUniform));

		cameraUniform = RT::Uniform::create(RT::UniformType::Uniform, sizeof(RT::Camera::Spec));
		cameraUniform->setData(&camera.getSpec(), sizeof(RT::Camera::Spec));

		materialsStorage = RT::Uniform::create(RT::UniformType::Storage, scene.materials.size() > 0 ? sizeof(RT::Material) * scene.materials.size() : 1);
		materialsStorage->setData(scene.materials.data(), sizeof(RT::Material) * scene.materials.size());

		spheresStorage = RT::Uniform::create(RT::UniformType::Storage, sceneWrapper.spheres.size() > 0 ? sizeof(Sphere) * sceneWrapper.spheres.size() : 1);
		spheresStorage->setData(sceneWrapper.spheres.data(), sizeof(Sphere) * sceneWrapper.spheres.size());

		bvhStorage = RT::Uniform::create(RT::UniformType::Storage, sceneWrapper.boundingBoxes.size() > 0 ? sizeof(BoundingBox) * sceneWrapper.boundingBoxes.size() : 1);
		bvhStorage->setData(sceneWrapper.boundingBoxes.data(), sizeof(BoundingBox) * sceneWrapper.boundingBoxes.size());

		trianglesStorage = RT::Uniform::create(RT::UniformType::Storage, sceneWrapper.triangles.size() > 0 ? sizeof(RT::Triangle) * sceneWrapper.triangles.size() : 1);
		trianglesStorage->setData(sceneWrapper.triangles.data(), sizeof(RT::Triangle) * sceneWrapper.triangles.size());

		meshWrappersStorage = RT::Uniform::create(RT::UniformType::Storage, sceneWrapper.meshWrappers.size() > 0 ? sizeof(MeshWrapper) * sceneWrapper.meshWrappers.size() : 1);
		meshWrappersStorage->setData(sceneWrapper.meshWrappers.data(), sizeof(MeshWrapper) * sceneWrapper.meshWrappers.size());

		meshInstanceWrappersStorage = RT::Uniform::create(RT::UniformType::Storage, sceneWrapper.meshInstanceWrappers.size() > 0 ? sizeof(MeshInstanceWrapper) * sceneWrapper.meshInstanceWrappers.size() : 1);
		meshInstanceWrappersStorage->setData(sceneWrapper.meshInstanceWrappers.data(), sizeof(MeshInstanceWrapper) * sceneWrapper.meshInstanceWrappers.size());

		auto pipelineSpec = RT::PipelineSpec{};
		pipelineSpec.shaderPath = assetDir / "shaders" / "RayTracing.shader";
		pipelineSpec.uniformLayouts = RT::UniformLayouts{
			{.nrOfSets = 1, .layout = {
				{.type = RT::UniformType::Image,	.count = 1 },
				{.type = RT::UniformType::Image,	.count = 1 },
				{.type = RT::UniformType::Sampler, .count = 1 },
				{.type = RT::UniformType::Uniform, .count = 1 },
				{.type = RT::UniformType::Uniform, .count = 1 } } },
			{.nrOfSets = 1, .layout = {
				{.type = RT::UniformType::Storage, .count = 1 },
				{.type = RT::UniformType::Storage, .count = 1 },
				{.type = RT::UniformType::Storage, .count = 1 },
				{.type = RT::UniformType::Storage, .count = 1 },
				{.type = RT::UniformType::Storage, .count = 1 },
				{.type = RT::UniformType::Storage, .count = 1 },
				{.type = RT::UniformType::Sampler, .count = 0 < (uint32_t)textures.size() ? (uint32_t)textures.size() : 1 } } }
		};
		pipelineSpec.attachmentFormats = {};
		pipeline = RT::Pipeline::create(pipelineSpec);

		pipeline->updateSet(0, 0, 0, *accumulationTexture);
		pipeline->updateSet(0, 0, 1, *outTexture);
		pipeline->updateSet(0, 0, 2, *skyMap);
		pipeline->updateSet(0, 0, 3, *ammountsUniform);
		pipeline->updateSet(0, 0, 4, *cameraUniform);
		pipeline->updateSet(1, 0, 0, *materialsStorage);
		pipeline->updateSet(1, 0, 1, *spheresStorage);
		pipeline->updateSet(1, 0, 2, *bvhStorage);
		pipeline->updateSet(1, 0, 3, *trianglesStorage);
		pipeline->updateSet(1, 0, 4, *meshWrappersStorage);
		pipeline->updateSet(1, 0, 5, *meshInstanceWrappersStorage);
		if (0 < textures.size())
		{
			pipeline->updateSet(1, 0, 6, textures);
		}
	}

private:
	ImVec2 viewportSize;
	float lastFrameDuration;
	glm::vec2 lastMousePos;
	glm::ivec2 lastWinSize;
	int32_t selectedScene;

	RT::Camera camera;
	RT::Scene scene;
	SceneWrapper sceneWrapper;

	RT::Local<RT::Texture> accumulationTexture;
	RT::Local<RT::Texture> outTexture;
	RT::Local<RT::Texture> skyMap;
	RT::TextureArray textures;

	RT::Local<RT::Uniform> cameraUniform;
	RT::Local<RT::Uniform> ammountsUniform;
	RT::Local<RT::Uniform> materialsStorage;
	RT::Local<RT::Uniform> spheresStorage;
	RT::Local<RT::Uniform> bvhStorage;
	RT::Local<RT::Uniform> trianglesStorage;
	RT::Local<RT::Uniform> meshWrappersStorage;
	RT::Local<RT::Uniform> meshInstanceWrappersStorage;

	RT::Local<RT::Pipeline> pipeline;

	bool accumulation = false;
	bool drawEnvironmentTranslator = false;

	struct InfoUniform
	{
		float drawEnvironment = (float)false;
		uint32_t maxBounces = 1;
		uint32_t maxFrames = 1;
		uint32_t frameIndex = 1;
		glm::vec2 resolution = {};
		int32_t materialsCount = 0;
		int32_t spheresCount = 0;
		int32_t objectsCount = 0;
		int32_t texturesCount = 0;
		uint32_t debug = 0;
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
