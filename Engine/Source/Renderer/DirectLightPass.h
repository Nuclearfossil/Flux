#pragma once

#include "RenderPhase.h"

#include "AddPass.h"
#include "Renderer/GBuffer.h"
#include "Framebuffer.h"

#include <memory>

namespace Flux
{
    class Texture2D;

    class DirectLightPass : public RenderPhase
    {
    public:
        DirectLightPass();

        void SetGBuffer(const GBuffer* gBuffer);

        void Resize(const Size& windowSize) override;

        void render(RenderState& renderState, const Scene& scene) override;

    private:
        ShaderProgram shader;

        const GBuffer* gBuffer;

        const Texture2D ampTex;
        const Texture2D matTex;

        Framebuffer buffer;
        Texture2D lightTex;

        AddPass addPass;
    };
}
