#pragma once

#include "RenderPhase.h"

#include <memory>

namespace Flux
{
    class Texture2D;
    class Texture3D;

    class FogPass : public RenderPhase
    {
    public:
        FogPass();

        void SetDepthMap(const Texture2D* depthMap);
        void SetFogColor(const Vector3f fogColor);

        void Resize(const Size& windowSize) override;

        void render(RenderState& renderState, const Scene& scene) override;

    private:
        ShaderProgram shader;

        const Texture2D* depthMap;

        Vector3f fogColor;
    };
}
