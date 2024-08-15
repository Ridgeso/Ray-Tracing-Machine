#include "GlfwWindow.h"

#include "Engine/Render/RenderApi.h"
#include "Engine/Event/Event.h"
#include "Engine/Event/AppEvents.h"

#include <imgui.h>
#include "External/window/ImGuiImpl.h"

namespace
{
    
    const char* glfwError2String(const int32_t error)
    {
        switch (error)
        {
            case GLFW_NO_ERROR:            return "GLFW_NO_ERROR";
            case GLFW_NOT_INITIALIZED:     return "GLFW_NOT_INITIALIZED";
            case GLFW_NO_CURRENT_CONTEXT:  return "GLFW_NO_CURRENT_CONTEXT";
            case GLFW_INVALID_ENUM:        return "GLFW_INVALID_ENUM";
            case GLFW_INVALID_VALUE:       return "GLFW_INVALID_VALUE";
            case GLFW_OUT_OF_MEMORY:       return "GLFW_OUT_OF_MEMORY";
            case GLFW_API_UNAVAILABLE:     return "GLFW_API_UNAVAILABLE";
            case GLFW_VERSION_UNAVAILABLE: return "GLFW_VERSION_UNAVAILABLE";
            case GLFW_PLATFORM_ERROR:      return "GLFW_PLATFORM_ERROR";
            case GLFW_FORMAT_UNAVAILABLE:  return "GLFW_FORMAT_UNAVAILABLE";
            case GLFW_NO_WINDOW_CONTEXT:   return "GLFW_NO_WINDOW_CONTEXT";
        }
        return "not found";
    }

    void GLFWErrorCallback(int32_t error, const char* description)
    {
        RT_LOG_ERROR("GLFW Callback {}: {}", glfwError2String(error), description);
    }

    void closeWindow(GLFWwindow* /*window*/)
    {
        auto event = RT::Event::Event<RT::Event::AppClose>{};
        event.process();
    }
    
    void windowResize(GLFWwindow* window, int32_t width, int32_t height)
    {
        auto& data = *(RT::GlfwWindow::Context*)glfwGetWindowUserPointer(window);
        data.size = { width, height };
        data.isMinimized = 0 == width && 0 == height;

        auto event = RT::Event::Event<RT::Event::WindowResize>{};
        event.fill([&data](auto& e)
        {
            e.width = data.size.x;
            e.height = data.size.y;
            e.isMinimized = data.isMinimized;
        });
        event.process();
    }

}

namespace RT
{

    void GlfwWindow::init(const WindowSpecs& specs)
    {
        if (!glfwInit())
        {
            return;
        }
        if (RenderApi::api == RenderApi::Api::Vulkan)
        {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        }

        context.title = specs.titel;
        context.size.x = specs.width;
        context.size.y = specs.height;
        context.isMinimized = specs.isMinimized;

        window = glfwCreateWindow(context.size.x, context.size.y, context.title.c_str(), NULL, NULL);
        if (!window)
        {
            glfwTerminate();
            return;
        }
        glfwSetWindowUserPointer(window, &context);

        glfwMakeContextCurrent(window);
        glfwSwapInterval(0);

        setCallbacks();

        initImGui();
    }

    void GlfwWindow::shutDown()
    {
        ImGuiImpl::shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void GlfwWindow::setTitleBar(const std::string& title)
    {
        glfwSetWindowTitle(window, title.c_str());
    }

    void GlfwWindow::update()
    {
        glfwPollEvents();
        //glfwSwapBuffers(window);
    }

    void GlfwWindow::beginUI()
    {
        ImGuiImpl::begin();
        ImGui::NewFrame();

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    }

    void GlfwWindow::endUI()
    {
        ImGui::Render();
        ImGuiImpl::end();

        ImGuiIO& io = ImGui::GetIO(); (void)io;
        
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backupCurrentContext = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backupCurrentContext);
        }
    }

    glm::vec2 GlfwWindow::getMousePos() const
    {
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        return { (float)x, (float)y };
    }

    bool GlfwWindow::isKeyPressed(int32_t key) const
    {
        return glfwGetKey(window, key) == GLFW_PRESS;
    }

    bool GlfwWindow::isMousePressed(int32_t key) const
    {
        return glfwGetMouseButton(window, key) == GLFW_PRESS;
    }

    glm::ivec2 GlfwWindow::getSize() const
    {
        int32_t width, height;
        glfwGetWindowSize(window, &width, &height);
        return { width, height };
    }

    void GlfwWindow::cursorMode(int32_t state) const
    {
        glfwSetInputMode(window, GLFW_CURSOR, state);
    }

    void GlfwWindow::setCallbacks()
    {
        glfwSetErrorCallback(GLFWErrorCallback);
        glfwSetWindowCloseCallback(window, closeWindow);
        glfwSetWindowSizeCallback(window, windowResize);
        //glfwSetKeyCallback(window, keyCallback);
        //glfwSetCharCallback(window, charCallback);
        //glfwSetMouseButtonCallback(window, mouseButton);
        //glfwSetScrollCallback(window, scroll);
        //glfwSetCursorPosCallback(window, cursorPos);
    }

    void GlfwWindow::initImGui()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        auto& io = ImGui::GetIO(); (void)io;

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard
            | ImGuiConfigFlags_DockingEnable
            | ImGuiConfigFlags_ViewportsEnable;

        ImGui::StyleColorsClassic();

        auto& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        ImGuiImpl::initImGui(window);
    }

}
