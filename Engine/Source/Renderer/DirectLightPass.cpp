#include "Renderer/DirectLightPass.h"

#include "Renderer/RenderState.h"

#include "TextureUnit.h"
#include "Texture.h"
#include "Framebuffer.h"
#include "Util/Math.h"
#include "GGX.h"

#include "DirectionalLight.h"
#include "AreaLight.h"

#include <GDT/Matrix4f.h>

namespace Flux {
    namespace
    {
        const Texture2D createAmplitudeTex()
        {
            Texture2D ampTex;
            ampTex.create();
            ampTex.bind(TextureUnit::TEXTURE0);
            ampTex.setData(32, 32, GL_RG32F, GL_RG, GL_FLOAT, amp.data());
            ampTex.setWrapping(CLAMP, CLAMP);
            ampTex.setSampling(LINEAR, LINEAR);

            return ampTex;
        }

        const Texture2D createMatrixTex()
        {
            std::vector<float> data;
            data.reserve(a.size() + b.size() + c.size() + d.size());
            for (int i = 0; i < 64 * 64; i++) {
                data.push_back(a[i]);
                data.push_back(b[i]);
                data.push_back(c[i]);
                data.push_back(d[i]);
            }

            Texture2D matTex;
            matTex.create();
            matTex.bind(TextureUnit::TEXTURE0);
            matTex.setData(64, 64, GL_RGBA32F, GL_RGBA, GL_FLOAT, data.data());
            matTex.setWrapping(CLAMP, CLAMP);
            matTex.setSampling(LINEAR, LINEAR);

            return matTex;
        }
    }

    DirectLightPass::DirectLightPass()
        :
        RenderPhase("Direct Lighting"),
        ampTex(createAmplitudeTex()),
        matTex(createMatrixTex())
    {
        shader.loadFromFile("res/Shaders/Quad.vert", "res/Shaders/DeferredDirect.frag");

        requiredSet.addCapability(STENCIL_TEST, true);
        requiredSet.addCapability(DEPTH_TEST, false);
    }

    void DirectLightPass::SetGBuffer(const GBuffer* gBuffer)
    {
        this->gBuffer = gBuffer;
    }

    void DirectLightPass::Resize(const Size& windowSize)
    {
        lightTex.create();
        lightTex.bind(TextureUnit::TEXTURE0);
        lightTex.setData(windowSize.width, windowSize.height, GL_RGBA16F, GL_RGBA, GL_FLOAT, nullptr);
        lightTex.setWrapping(CLAMP, CLAMP);
        lightTex.setSampling(LINEAR, LINEAR);
        lightTex.release();

        buffer.create();
        buffer.bind();
        buffer.addColorTexture(0, lightTex);
        buffer.validate();
        buffer.release();
    }

    void DirectLightPass::render(RenderState& renderState, const Scene& scene)
    {
        renderState.require(requiredSet);

        nvtxRangePushA(getPassName().c_str());

        const Framebuffer* sourceFramebuffer = RenderState::currentFramebuffer;

        buffer.bind();

        shader.bind();

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        renderState.enable(BLENDING);
        glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ZERO);
        glStencilFunc(GL_EQUAL, 1, 0xFF);

        Entity* camera = scene.getMainCamera();
        Transform& ct = camera->getComponent<Transform>();

        shader.uniform3f("camPos", ct.position);

        gBuffer->albedoTex.bind(TextureUnit::ALBEDO);
        shader.uniform1i("albedoMap", TextureUnit::ALBEDO);
        gBuffer->normalTex.bind(TextureUnit::NORMAL);
        shader.uniform1i("normalMap", TextureUnit::NORMAL);
        gBuffer->positionTex.bind(TextureUnit::POSITION);
        shader.uniform1i("positionMap", TextureUnit::POSITION);
        gBuffer->emissionTex.bind(TextureUnit::EMISSION);
        shader.uniform1i("emissionMap", TextureUnit::EMISSION);
        
        for (Entity* light : scene.lights) {
            Transform& transform = light->getComponent<Transform>();

            if (light->hasComponent<DirectionalLight>()) {
                DirectionalLight& directionalLight = light->getComponent<DirectionalLight>();

                Vector3f direction = Math::directionFromRotation(transform.rotation, Vector3f(0, 0, -1));

                shader.uniform3f("dirLight.direction", direction);
                shader.uniform3f("dirLight.color", directionalLight.color);
                shader.uniform1i("isDirLight", true);
                shader.uniform1i("isPointLight", false);
                shader.uniform1i("isAreaLight", false);
                directionalLight.shadowMap.bind(TextureUnit::SHADOW);
                shader.uniform1i("dirLight.shadowMap", TextureUnit::SHADOW);
                shader.uniformMatrix4f("dirLight.shadowMatrix", directionalLight.shadowSpace);
            }
            else if (light->hasComponent<PointLight>()) {
                PointLight& pointLight = light->getComponent<PointLight>();

                shader.uniform3f("pointLight.position", transform.position);
                shader.uniform3f("pointLight.color", pointLight.color);
                shader.uniform1i("isPointLight", true);
                shader.uniform1i("isDirLight", false);
                shader.uniform1i("isAreaLight", false);
                pointLight.shadowMap.bind(TextureUnit::TEXTURE7);
                shader.uniform1i("pointLight.shadowMap", TextureUnit::TEXTURE7);
            }
            else if (light->hasComponent<AreaLight>()) {
                AreaLight& areaLight = light->getComponent<AreaLight>();

                ampTex.bind(TextureUnit::TEXTURE3);
                matTex.bind(TextureUnit::TEXTURE4);
                shader.uniform1i("areaLight.ampTex", TextureUnit::TEXTURE3);
                shader.uniform1i("areaLight.matTex", TextureUnit::TEXTURE4);

                Matrix4f modelMatrix;
                modelMatrix.setIdentity();

                transform.rotation.z += 0.5f;
                modelMatrix.translate(transform.position);
                modelMatrix.rotate(transform.rotation);
                modelMatrix.scale(transform.scale);

                std::vector<Vector3f> vertices;
                for (Vector3f vertex : areaLight.vertices) {
                    vertices.push_back(modelMatrix.transform(vertex, 1));
                }

                shader.uniform3f("areaLight.color", areaLight.color);
                shader.uniform3fv("areaLight.vertices", vertices.size(), vertices.data());
                shader.uniform1i("isPointLight", false);
                shader.uniform1i("isDirLight", false);
                shader.uniform1i("isAreaLight", true);
            }
            else {
                continue;
            }

            renderState.drawQuad();
        }

        renderState.disable(BLENDING);

        buffer.release();

        sourceFramebuffer->bind();

        // Add the direct light to the original buffer
        std::vector<Texture2D> sources{ lightTex, *source };
        std::vector<float> weights{ 1, 1 };
        addPass.SetTextures(sources);
        addPass.SetWeights(weights);
        addPass.render(renderState, scene);

        nvtxRangePop();
    }
}
