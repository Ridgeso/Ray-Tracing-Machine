#include "ImGuiImpl.h"
#include <vector>

#include <imgui.h>

#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

#include "External/Render/Vulkan/Device.h"

#include "Engine/Core/Assert.h"
#include "Engine/Render/RenderApi.h"

namespace RT::ImGuiImpl
{

    void initImGui(GLFWwindow* window)
    {
        switch (RenderApi::api)
        {
            //case RenderApi::Api::OpenGL:
            //    ImGui_ImplGlfw_InitForOpenGL(window, true);
            //    ImGui_ImplOpenGL3_Init("#version 450 core");
            //    ImGui::GetIO().Fonts->Build();
            //    break;
            case RenderApi::Api::Vulkan:
                break;
        }
    }

    void shutdown()
    {
        switch (RenderApi::api)
        {
            //case RenderApi::Api::OpenGL:
            //    ImGui_ImplOpenGL3_Shutdown();
            //    break;
            case RenderApi::Api::Vulkan:
                break;
        }
        ImGui_ImplGlfw_Shutdown();
    }

    void begin()
    {
        switch (RenderApi::api)
        {
            //case RenderApi::Api::OpenGL:
            //    ImGui_ImplOpenGL3_NewFrame();
            //    ImGui_ImplGlfw_NewFrame();
            //    break;
            case RenderApi::Api::Vulkan:
                ImGui_ImplVulkan_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                break;
        }
    }

    void end()
    {
        switch (RenderApi::api)
        {
            //case RenderApi::Api::OpenGL:
            //    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            //    break;
            case RenderApi::Api::Vulkan:
                break;
        }
    }

}
