#pragma once
#include <string>
#include <cstdint>
#include <glm/glm.hpp>
#include <Engine/Core/Base.h>

namespace RT
{

	struct WindowSpecs
	{
		std::string titel;
		int32_t width, height;
		bool isMinimized;
	};

	class Window
	{
		friend class Application;

	public:
		virtual void init(const WindowSpecs& specs) = 0;
		virtual void shutDown() = 0;
		
		virtual void setTitleBar(const std::string& title) = 0;

		virtual glm::vec2 getMousePos() const = 0;
		virtual bool isKeyPressed(int32_t key) const = 0;
		virtual bool isMousePressed(int32_t key) const = 0;
		virtual glm::ivec2 getSize() const = 0;

		virtual void cursorMode(int32_t state) const = 0;

		virtual void* getNativWindow() = 0;

		static Local<Window>& instance() { return window; }
		
	private:
		virtual bool update() = 0;
		virtual bool pullEvents() = 0;

		virtual void beginUI() = 0;
		virtual void endUI() = 0;

	private:
		static Local<Window> window;
	};

}
