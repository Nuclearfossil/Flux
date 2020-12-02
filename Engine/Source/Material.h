#pragma once

#include "Texture.h"

#include <GDT/Vector3f.h>

namespace GDT
{
    class ShaderProgram;
}

using GDT::ShaderProgram;

namespace Flux {
    class Material {
    public:
        Material()
            :
            emission(0, 0, 0),
            tilingX(1), tilingY(1)
        {

        }

        Texture2D diffuseTex;
        Texture2D normalTex;
        Texture2D metalTex;
        Texture2D roughnessTex;
        Texture2D stencilTex;
        Texture2D emissionTex;
        GDT::Vector3f emission;
        float tilingX, tilingY;

        void bind(ShaderProgram& shader) const;
        void release(ShaderProgram& shader) const;
    };
}
