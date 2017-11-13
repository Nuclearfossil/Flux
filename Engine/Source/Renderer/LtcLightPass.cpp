#include "Renderer/LtcLightPass.h"

#include "Renderer/RenderState.h"

#include "TextureUnit.h"
#include "Texture.h"
#include "Framebuffer.h"
#include "TextureLoader.h"
#include "Entity.h"
#include "Mesh.h"
#include "MeshRenderer.h"
#include "GGX.h"

#include "DirectionalLight.h"
#include "AttachedTo.h"
#include <iostream>

namespace Flux {
    LtcLightPass::LtcLightPass() : RenderPhase("Area Lighting")
    {
        shader = std::unique_ptr<Shader>(Shader::fromFile("res/Shaders/Quad.vert", "res/Shaders/DeferredArea.frag"));

        ampTex = TextureLoader::create(64, 64, GL_R32F, GL_RED, GL_FLOAT, Sampling::LINEAR, false, amp.data());
        aTex = TextureLoader::create(64, 64, GL_R32F, GL_RED, GL_FLOAT, Sampling::LINEAR, false, a.data());
        bTex = TextureLoader::create(64, 64, GL_R32F, GL_RED, GL_FLOAT, Sampling::LINEAR, false, b.data());
        cTex = TextureLoader::create(64, 64, GL_R32F, GL_RED, GL_FLOAT, Sampling::LINEAR, false, c.data());
        dTex = TextureLoader::create(64, 64, GL_R32F, GL_RED, GL_FLOAT, Sampling::LINEAR, false, d.data());
    }

    void LtcLightPass::SetGBuffer(const GBuffer* gBuffer)
    {
        this->gBuffer = gBuffer;
    }

    void LtcLightPass::SetTarget(const Framebuffer* target)
    {
        this->target = target;
    }

    void LtcLightPass::render(const Scene& scene)
    {
        nvtxRangePushA(getPassName().c_str());

        shader->bind();

        RenderState::enable(BLENDING);
        glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ZERO);
        glDepthFunc(GL_LEQUAL);

        ///
        Entity* camera = scene.getMainCamera();
        Transform* ct = camera->getComponent<Transform>();
        Camera* cam = camera->getComponent<Camera>();

        // Set the projection matrix from the camera parameters
        Matrix4f projMatrix;
        camera->getComponent<Camera>()->loadProjectionMatrix(projMatrix);

        // Set the view matrix to the camera view
        Matrix4f viewMatrix;
        viewMatrix.setIdentity();
        viewMatrix.rotate(-ct->rotation);
        viewMatrix.translate(-ct->position);

        shader->uniform3f("camPos", ct->position);
        shader->uniformMatrix4f("projMatrix", projMatrix);
        shader->uniformMatrix4f("viewMatrix", viewMatrix);
        shader->uniform1f("zNear", cam->getZNear());
        shader->uniform1f("zFar", cam->getZFar());
        ///

        gBuffer->albedoTex->bind(TextureUnit::ALBEDO);
        shader->uniform1i("albedoMap", TextureUnit::ALBEDO);
        gBuffer->normalTex->bind(TextureUnit::NORMAL);
        shader->uniform1i("normalMap", TextureUnit::NORMAL);
        gBuffer->positionTex->bind(TextureUnit::POSITION);
        shader->uniform1i("positionMap", TextureUnit::POSITION);

        ampTex->bind(TextureUnit::TEXTURE3);
        aTex->bind(TextureUnit::TEXTURE4);
        bTex->bind(TextureUnit::TEXTURE5);
        cTex->bind(TextureUnit::TEXTURE6);
        dTex->bind(TextureUnit::TEXTURE7);
        shader->uniform1i("ampTex", TextureUnit::TEXTURE3);
        shader->uniform1i("aTex", TextureUnit::TEXTURE4);
        shader->uniform1i("bTex", TextureUnit::TEXTURE5);
        shader->uniform1i("cTex", TextureUnit::TEXTURE6);
        shader->uniform1i("dTex", TextureUnit::TEXTURE7);

        for (Entity* e : scene.entities) {
            if (!e->hasComponent<Mesh>())
                continue;

            if (e->hasComponent<MeshRenderer>()) {
                MeshRenderer* mr = e->getComponent<MeshRenderer>();
                Transform* transform = e->getComponent<Transform>();
                Material* material = scene.materials[mr->materialID];

                if (material && material->emission.length() > 0.01) {
                    Matrix4f modelMatrix;
                    modelMatrix.setIdentity();

                    if (e->hasComponent<AttachedTo>()) {
                        Entity* parent = scene.getEntityById(e->getComponent<AttachedTo>()->parentId);

                        if (parent != nullptr) {
                            Transform* parentT = parent->getComponent<Transform>();
                            modelMatrix.translate(parentT->position);
                            modelMatrix.rotate(parentT->rotation);
                            modelMatrix.scale(parentT->scale);
                        }
                    }

                    modelMatrix.translate(transform->position);
                    modelMatrix.rotate(transform->rotation);
                    modelMatrix.scale(transform->scale);

                    Mesh* mesh = e->getComponent<Mesh>();

                    std::vector<Vector3f> vertices;
                    for (int i = 0; i < mesh->vertices.size(); i++) {
                        vertices.push_back(modelMatrix.transform(mesh->vertices[i], 1));
                    }

                    shader->uniform1i("numVertices", vertices.size());
                    shader->uniform3fv("vertices", vertices.size(), vertices.data());
                    shader->uniform3f("emission", material->emission);

                    RenderState::drawQuad();
                }
            }
        }

        RenderState::disable(BLENDING);

        nvtxRangePop();
    }
}