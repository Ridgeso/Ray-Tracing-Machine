#include "ImGuiImpl.h"
#include <vector>

#include <imgui.h>

#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

#include "External/render/Vulkan/Device.h"

#include "Engine/Core/Assert.h"
#include "Engine/render/RenderBase.h"

namespace RT::ImGuiImpl
{

    void initImGui(GLFWwindow* window)
    {
        switch (GlobalRenderAPI)
        {
            case RT::RenderAPI::OpenGL:
                ImGui_ImplGlfw_InitForOpenGL(window, true);
                ImGui_ImplOpenGL3_Init("#version 450 core");
                ImGui::GetIO().Fonts->Build();
                break;
            case RT::RenderAPI::Vulkan:
                break;
        }
    }

    void shutdown()
    {
        switch (GlobalRenderAPI)
        {
            case RT::RenderAPI::OpenGL:
                ImGui_ImplOpenGL3_Shutdown();
                break;
            case RT::RenderAPI::Vulkan:
                break;
        }
        ImGui_ImplGlfw_Shutdown();
    }

    void begin()
    {
        switch (GlobalRenderAPI)
        {
            case RT::RenderAPI::OpenGL:
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                break;
            case RT::RenderAPI::Vulkan:
                ImGui_ImplVulkan_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                break;
        }
    }

    void end()
    {
        switch (GlobalRenderAPI)
        {
            case RT::RenderAPI::OpenGL:
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                break;
            case RT::RenderAPI::Vulkan:
                break;
        }
    }

}
