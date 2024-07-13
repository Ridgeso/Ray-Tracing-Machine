#include "GlfwWindow.h"
#include <vector>
#include "Engine/Core/Assert.h"

#include <imgui.h>
#include <glad/glad.h>

#include "Engine/Render/RenderBase.h"

#include "External/window/ImGuiImpl.h"


namespace RT
{

    void GlfwWindow::init(const WindowSpecs& specs)
    {
        if (!glfwInit())
        {
            return;
        }
        if (GlobalRenderAPI == RenderAPI::Vulkan)
        {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        }

        title = specs.titel;
        width = specs.width;
        height = specs.height;
        isMinimized = specs.isMinimized;

        window = glfwCreateWindow(width, height, title.c_str(), NULL, NULL);
        if (!window)
        {
            glfwTerminate();
            return;
        }

        glfwMakeContextCurrent(window);
        glfwSwapInterval(0);

        initImGui();
    }

    void GlfwWindow::shutDown()
    {
        ImGuiImpl::shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    bool GlfwWindow::update()
    {
        glfwPollEvents();
        glfwSwapBuffers(window);

        return !glfwWindowShouldClose(window);
    }

    bool GlfwWindow::pullEvents()
    {
        return true;
    }

    void GlfwWindow::beginUI()
    {
        ImGuiImpl::begin();
        ImGui::NewFrame();

        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
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
