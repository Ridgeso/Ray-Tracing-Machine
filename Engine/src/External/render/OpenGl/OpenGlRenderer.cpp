#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <execution>
#include <cstddef>

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h>

#include "OpenGlRenderer.h"

namespace RT::OpenGl
{

    void OpenGlRenderer::init(const RenderSpecs& specs)
    {
        loadOpenGlForGlfw();

        this->specs = specs;
    }

    void OpenGlRenderer::shutDown()
    {
    }

    void OpenGlRenderer::render(
        const RenderPass& renderPass,
        const Camera& camera,
        //const Shader& shader,
        const VertexBuffer& vbuffer,
        const Scene& scene)
    {
        /*
        const auto& openGlRenderPass = static_cast<const OpenGlRenderPass&>(renderPass);
        const auto& openGlShader = static_cast<const OpenGlShader&>(shader);
        const auto& openGlVbuffer = static_cast<const OpenGlVertexBuffer&>(vbuffer);

        openGlRenderPass.bind();
        openGlShader.use();
        openGlVbuffer.bind();

        glDrawArrays(GL_TRIANGLES, 0, openGlVbuffer.getCount());

        openGlVbuffer.unbind();
        openGlShader.unuse();
        openGlRenderPass.unbind();
        */
    }

    void OpenGlRenderer::loadOpenGlForGlfw()
    {
        gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    }

}
