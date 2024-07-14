#pragma once
#include <cstdint>
#include <string>

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

#include "Engine/Window/Window.h"

namespace RT
{

	class GlfwWindow : public Window
	{
	public:
		GlfwWindow() = default;
		~GlfwWindow() = default;
		
		void init(const WindowSpecs& specs) final;
		void shutDown() final;

		void setTitleBar(const std::string& title) final;

		bool update() final;
		bool pullEvents() final;

		void beginUI() final;
		void endUI() final;

		glm::vec2 getMousePos() const final;
		bool isKeyPressed(int32_t key) const final;
		bool isMousePressed(int32_t key) const final;
		glm::ivec2 getSize() const final;

		void cursorMode(int32_t state) const final;

		void* getNativWindow() override { return window; }

	private:
		void initImGui();

	private:
		std::string title = "";
		int32_t width = 0, height = 0;

		bool isMinimized = false;

		GLFWwindow* window = nullptr;
	};

}
