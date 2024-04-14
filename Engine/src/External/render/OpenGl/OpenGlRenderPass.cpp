#include "OpenGlRenderPass.h"

#include <glad/glad.h>


namespace RT::OpenGl
{

    OpenGlRenderPass::OpenGlRenderPass(const RenderPassSpec& spec)
		: renderId{}, frameId{}, size{spec.size}
	{
        glCreateFramebuffers(1, &frameId);
        glBindFramebuffer(GL_FRAMEBUFFER, frameId);

        auto buffers = std::vector<uint32_t>(spec.attachmentsFormats.size());
        attachments.reserve(spec.attachmentsFormats.size());
        uint32_t attachmentsSize = 0;
        for (int32_t nrOfAttach = 0; nrOfAttach < spec.attachmentsFormats.size(); nrOfAttach++)
        {
            if (ImageFormat::Depth == spec.attachmentsFormats[nrOfAttach])
            {
                continue;
            }

            buffers[nrOfAttach] = GL_COLOR_ATTACHMENT0 + nrOfAttach;
            attachments.emplace_back(size, spec.attachmentsFormats[nrOfAttach]);
            glFramebufferTexture2D(
                GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0 + nrOfAttach,
                GL_TEXTURE_2D,
                attachments[nrOfAttach].getId(),
                0);
            attachmentsSize++;
        }

        glGenRenderbuffers(1, &renderId);
        glBindRenderbuffer(GL_RENDERBUFFER, renderId);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size.x, size.y);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderId);

        glDrawBuffers(attachmentsSize, buffers.data());

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

    OpenGlRenderPass::~OpenGlRenderPass()
	{
        glDeleteRenderbuffers(1, &renderId);
        glDeleteFramebuffers(1, &frameId);
	}

    const Texture& OpenGlRenderPass::getAttachment(const uint32_t index) const
    {
        return attachments[index];
    }

    void OpenGlRenderPass::bind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, frameId);
        glViewport(0, 0, size.x, size.y);
    }

    void OpenGlRenderPass::unbind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

}
