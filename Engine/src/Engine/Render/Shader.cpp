#include "RenderBase.h"
#include "Shader.h"
#include "External/Render/OpenGl/OpenGlShader.h"
#include "External/Render/Vulkan/VulkanShader.h"

namespace RT
{

    Local<Shader> RT::createShader()
    {
        switch (GlobalRenderAPI)
        {
            // case RenderAPI::OpenGL: return makeLocal<OpenGl::OpenGlShader>();
            case RenderAPI::Vulkan: return makeLocal<Vulkan::VulkanShader>();
        }
        return nullptr;
    }

}
