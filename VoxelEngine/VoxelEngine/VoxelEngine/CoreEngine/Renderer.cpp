#include "EnginePCH.h"
#include "Renderer.h"

#include <CoreEngine/WindowUtils.h>
#include <CoreEngine/Window.h>
#include <CoreEngine/shader.h>
#include <CoreEngine/Camera.h>
#include <CoreEngine/Texture2D.h>
#include <CoreEngine/Mesh.h>
#include <CoreEngine/TextureCube.h>

#include <execution>

static void GLAPIENTRY
GLerrorCB(GLenum source,
  GLenum type,
  GLuint id,
  GLenum severity,
  GLsizei length,
  const GLchar* message,
  [[maybe_unused]] const void* userParam)
{
  //return; // UNCOMMENT WHEN DEBUGGING GRAPHICS

  // ignore non-significant error/warning codes
  if (id == 131169 || id == 131185 || id == 131218 || id == 131204
    )//|| id == 131188 || id == 131186)
    return;

  std::cout << "---------------" << std::endl;
  std::cout << "Debug message (" << id << "): " << message << std::endl;

  switch (source)
  {
  case GL_DEBUG_SOURCE_API:             std::cout << "Source: API"; break;
  case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   std::cout << "Source: Window Manager"; break;
  case GL_DEBUG_SOURCE_SHADER_COMPILER: std::cout << "Source: Shader Compiler"; break;
  case GL_DEBUG_SOURCE_THIRD_PARTY:     std::cout << "Source: Third Party"; break;
  case GL_DEBUG_SOURCE_APPLICATION:     std::cout << "Source: Application"; break;
  case GL_DEBUG_SOURCE_OTHER:           std::cout << "Source: Other"; break;
  } std::cout << std::endl;

  switch (type)
  {
  case GL_DEBUG_TYPE_ERROR:               std::cout << "Type: Error"; break;
  case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: std::cout << "Type: Deprecated Behaviour"; break;
  case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  std::cout << "Type: Undefined Behaviour"; break;
  case GL_DEBUG_TYPE_PORTABILITY:         std::cout << "Type: Portability"; break;
  case GL_DEBUG_TYPE_PERFORMANCE:         std::cout << "Type: Performance"; break;
  case GL_DEBUG_TYPE_MARKER:              std::cout << "Type: Marker"; break;
  case GL_DEBUG_TYPE_PUSH_GROUP:          std::cout << "Type: Push Group"; break;
  case GL_DEBUG_TYPE_POP_GROUP:           std::cout << "Type: Pop Group"; break;
  case GL_DEBUG_TYPE_OTHER:               std::cout << "Type: Other"; break;
  } std::cout << std::endl;

  switch (severity)
  {
  case GL_DEBUG_SEVERITY_HIGH:         std::cout << "Severity: high"; break;
  case GL_DEBUG_SEVERITY_MEDIUM:       std::cout << "Severity: medium"; break;
  case GL_DEBUG_SEVERITY_LOW:          std::cout << "Severity: low"; break;
  case GL_DEBUG_SEVERITY_NOTIFICATION: std::cout << "Severity: notification"; break;
  } std::cout << std::endl;
  std::cout << std::endl;
}

//glm::mat4 MVP = CameraSystem::GetViewProj() * modelMatrix;
void Renderer::BeginBatch(uint32_t size)
{
  userCommands.resize(size);
}

void Renderer::Submit(const Components::Transform& model, const Components::BatchedMesh& mesh, const Components::Material& mat)
{
  auto index = cmdIndex.fetch_add(1, std::memory_order::memory_order_acq_rel);
  userCommands[index] = BatchDrawCommand{ .mesh = mesh.handle->handle, .material = mat.handle->handle, .modelUniform = model.GetModel() };
}

void Renderer::RenderBatch()
{
  userCommands.resize(cmdIndex);
  //cmdIndex.store(0, std::memory_order_release);
  cmdIndex = 0;
  if (userCommands.empty())
  {
    return;
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_TRUE);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);

  // NOTE: mesh order MUST match meshBufferInfo's
  std::sort(std::execution::par, userCommands.begin(), userCommands.end(),
    [](const auto& lhs, const auto& rhs)
    {
      if (lhs.material != rhs.material)
        return lhs.material < rhs.material;
      else
        return lhs.mesh < rhs.mesh;
    });

  // accumulate per-material draws and uniforms
  std::vector<UniformData> uniforms;
  uniforms.reserve(userCommands.size());
  MaterialID curMat = userCommands[0].material;
  for (size_t i = 0; i < userCommands.size(); i++)
  {
    const auto& draw = userCommands[i];
    if (draw.material != curMat)
    {
      RenderBatchHelper(curMat, uniforms); // submit draw when material is done
      curMat = draw.material;
      uniforms.clear();
    }

    meshBufferInfo[draw.mesh].instanceCount++;
    uniforms.push_back(UniformData{ .model = draw.modelUniform });
  }
  if (uniforms.size() > 0)
  {
    RenderBatchHelper(curMat, uniforms);
  }

  userCommands.clear();
}


void Renderer::RenderBatchHelper(MaterialID mat, const std::vector<UniformData>& uniforms)
{
  // generate SSBO w/ uniforms
  auto uniformBuffer = std::make_unique<GFX::StaticBuffer>(uniforms.data(), uniforms.size() * sizeof(UniformData));
  uniformBuffer->Bind<GFX::Target::SSBO>(0);

  // generate DIB (one indirect command per mesh)
  std::vector<DrawElementsIndirectCommand> commands;
  GLuint baseInstance = 0;
  std::for_each(meshBufferInfo.begin(), meshBufferInfo.end(),
    [&commands, &baseInstance](auto& cmd)
    {
      if (cmd.second.instanceCount != 0)
      {
        cmd.second.baseInstance = baseInstance;
        //cmd.second.instanceCount += baseInstance;
        commands.push_back(cmd.second);
        baseInstance += cmd.second.instanceCount;
      }
    });
  GFX::StaticBuffer dib(commands.data(), commands.size() * sizeof(DrawElementsIndirectCommand));
  dib.Bind<GFX::Target::DIB>();

  // clear instance count for next GL draw command
  for (auto& info : meshBufferInfo)
  {
    info.second.instanceCount = 0;
  }

  // do the actual draw
  auto& material = MaterialManager::materials_[mat];
  auto& shader = Shader::shaders[material.shaderID];
  shader->Use();

  const auto& vp = CameraSystem::GetViewProj();
  shader->setMat4("u_viewProj", vp);

  batchVAO->Bind();
  glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, (void*)0, commands.size(), 0);
  batchVAO->Unbind();
}

void Renderer::BeginRenderParticleEmitter()
{
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  auto& shader = Shader::shaders["particle"];
  shader->Use();
  emptyVao->Bind();
  //glBlendFunc(GL_SRC_ALPHA, GL_ONE);
}

void Renderer::RenderParticleEmitter(const Components::ParticleEmitter& emitter, const Components::Transform& model)
{
  auto& shader = Shader::shaders["particle"];

  const auto& v = CameraSystem::GetView();
  shader->setMat4("u_viewProj", CameraSystem::GetViewProj());
  shader->setInt("u_sprite", 0);
  shader->setVec3("u_cameraRight", v[0][0], v[1][0], v[2][0]);
  shader->setVec3("u_cameraUp", v[0][1], v[1][1], v[2][1]);
  emitter.texture->Bind(0);
  emitter.particleBuffer->Bind<GFX::Target::SSBO>(0);

  glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, emitter.maxParticles);
}

void Renderer::Init()
{
  glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

  const int levels = glm::floor(glm::log2(glm::max(fboWidth, fboHeight))) + 1;
  glCreateTextures(GL_TEXTURE_2D, 1, &color);
  glTextureStorage2D(color, levels, GL_RGBA16F, fboWidth, fboHeight);

  glCreateTextures(GL_TEXTURE_2D, 1, &depth);
  glTextureStorage2D(depth, 1, GL_DEPTH_COMPONENT32F, fboWidth, fboHeight);

  glCreateFramebuffers(1, &fbo);
  glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, color, 0);
  glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, depth, 0);
  if (GLenum status = glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
  {
    fprintf(stderr, "glCheckNamedFramebufferStatus: %x\n", status);
  }

  exposureBuffer = std::make_unique<GFX::StaticBuffer>(&exposure, 2 * sizeof(float), GFX::BufferFlag::NONE);

  glEnable(GL_DEBUG_OUTPUT);
  glEnable(GL_DEPTH_TEST);

  // enable debugging stuff
  glDebugMessageCallback(GLerrorCB, NULL);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);
  glfwSwapInterval(0); // 0 == no vsync, 1 == vsync

  CompileShaders();

  // TODO: use dynamically sized buffer
  vertexBuffer = std::make_unique<GFX::DynamicBuffer<>>(100'000'000, sizeof(Vertex));
  indexBuffer = std::make_unique<GFX::DynamicBuffer<>>(100'000'000, sizeof(GLuint));

  // setup VAO for batched drawing (ONE VERTEX LAYOUT ATM)
  batchVAO = std::make_unique<GFX::VAO>();
  glEnableVertexArrayAttrib(batchVAO->GetID(), 0); // pos
  glEnableVertexArrayAttrib(batchVAO->GetID(), 1); // normal
  glEnableVertexArrayAttrib(batchVAO->GetID(), 2); // uv

  glVertexArrayAttribFormat(batchVAO->GetID(), 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
  glVertexArrayAttribFormat(batchVAO->GetID(), 1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
  glVertexArrayAttribFormat(batchVAO->GetID(), 2, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, texCoord));

  glVertexArrayAttribBinding(batchVAO->GetID(), 0, 0);
  glVertexArrayAttribBinding(batchVAO->GetID(), 1, 0);
  glVertexArrayAttribBinding(batchVAO->GetID(), 2, 0);

  glVertexArrayVertexBuffer(batchVAO->GetID(), 0, vertexBuffer->GetGPUHandle(), 0, sizeof(Vertex));
  glVertexArrayElementBuffer(batchVAO->GetID(), indexBuffer->GetGPUHandle());

  emptyVao = std::make_unique<GFX::VAO>();

  /*Layout layout = Window::layout;

  int width = layout.width;
  int height = layout.height;

  // Setup frameBuffer
  glGenFramebuffers(1, &RenderBuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, RenderBuffer);

  // Setup render texture
  glGenTextures(1, &RenderTexture);
  glBindTexture(GL_TEXTURE_2D, RenderTexture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, RenderTexture, 0);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
  {
    std::cout << "ERROR::FRAMEBUFFER:: Framebuffer in Raytrace Renderer is not complete!" << std::endl;
  }*/
}

void Renderer::CompileShaders()
{
  Shader::shaders["ShaderMcShaderFace"].emplace(Shader(
    {
      { "TexturedMesh.vs", GL_VERTEX_SHADER },
      { "TexturedMesh.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["batched"].emplace(Shader(
    {
      { "TexturedMeshBatched.vs", GL_VERTEX_SHADER },
      { "TexturedMesh.fs", GL_FRAGMENT_SHADER }
    }));

  Shader::shaders["chunk_optimized"].emplace(Shader(
    {
      { "chunk_optimized.vs", GL_VERTEX_SHADER },
      { "chunk_optimized.fs", GL_FRAGMENT_SHADER }
    }));
  //Shader::shaders["chunk_splat"] = new Shader("chunk_splat.vs", "chunk_splat.fs");
  Shader::shaders["compact_batch"].emplace(Shader(
    { { "compact_batch.cs", GL_COMPUTE_SHADER } }));
  Shader::shaders["update_particle_emitter"].emplace(Shader(
    { { "update_particle_emitter.cs", GL_COMPUTE_SHADER } }));
  Shader::shaders["update_particle"].emplace(Shader(
    { { "update_particle.cs", GL_COMPUTE_SHADER } }));
  //Shader::shaders["compact_batch"] = new Shader(0, "compact_batch.cs");
  Shader::shaders["textured_array"].emplace(Shader(
    {
      { "textured_array.vs", GL_VERTEX_SHADER },
      { "textured_array.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["buffer_vis"].emplace(Shader(
    {
      { "buffer_vis.fs", GL_FRAGMENT_SHADER },
      { "buffer_vis.vs", GL_VERTEX_SHADER }
    }));
  Shader::shaders["chunk_render_cull"].emplace(Shader(
    {
      { "chunk_render_cull.vs", GL_VERTEX_SHADER },
      { "chunk_render_cull.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["particle"].emplace(Shader(
    {
      { "particle.vs", GL_VERTEX_SHADER },
      { "particle.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["skybox"].emplace(Shader(
    {
      { "skybox.vs", GL_VERTEX_SHADER },
      { "skybox.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["tonemap"].emplace(Shader(
    {
      { "fullscreen_tri.vs", GL_VERTEX_SHADER },
      { "tonemap.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["calc_exposure"].emplace(Shader(
    { { "calc_exposure.cs", GL_COMPUTE_SHADER } }));

  Shader::shaders["sun"].emplace(Shader("flat_sun.vs", "flat_sun.fs"));
  Shader::shaders["axis"].emplace(Shader("axis.vs", "axis.fs"));
  Shader::shaders["flat_color"].emplace(Shader("flat_color.vs", "flat_color.fs"));
}

void Renderer::DrawAxisIndicator()
{
  static GFX::VAO* axisVAO;
  static GFX::StaticBuffer* axisVBO;
  if (axisVAO == nullptr)
  {
    float indicatorVertices[] =
    {
      // positions      // colors
      0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // x-axis
      1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, // y-axis
      0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, // z-axis
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f
    };

    axisVAO = new GFX::VAO();
    axisVBO = new GFX::StaticBuffer(indicatorVertices, sizeof(indicatorVertices));
    GFX::VBOlayout layout;
    layout.Push<float>(3);
    layout.Push<float>(3);
    axisVAO->AddBuffer(*axisVBO, layout);
  }
  auto& currShader = Shader::shaders["axis"];
  currShader->Use();
  //Camera* cam = Camera::ActiveCamera;
  currShader->setMat4("u_model", glm::translate(glm::mat4(1), CameraSystem::GetPos() + CameraSystem::GetFront() * 10.f)); // add scaling factor (larger # = smaller visual)
  currShader->setMat4("u_view", CameraSystem::GetView());
  currShader->setMat4("u_proj", CameraSystem::GetProj());
  glDepthFunc(GL_ALWAYS); // allows indicator to always be rendered
  axisVAO->Bind();
  glLineWidth(2.f);
  glDrawArrays(GL_LINES, 0, 6);
  glDepthFunc(GL_GEQUAL);
  axisVAO->Unbind();
}

// draws a single quad over the entire viewport
void Renderer::DrawQuad()
{
  static unsigned int quadVAO = 0;
  static unsigned int quadVBO;
  if (quadVAO == 0)
  {
    float quadVertices[] =
    {
      // positions        // texture Coords
      -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
      -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
       1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
       1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
    };
    // setup plane VAO
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
  }
  glBindVertexArray(quadVAO);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glBindVertexArray(0);
}

void Renderer::DrawCube()
{
  static GFX::VAO* blockHoverVao = nullptr;
  static GFX::StaticBuffer* blockHoverVbo = nullptr;
  if (blockHoverVao == nullptr)
  {
    blockHoverVao = new GFX::VAO();
    blockHoverVbo = new GFX::StaticBuffer(Vertices::cube_norm_tex, sizeof(Vertices::cube_norm_tex));
    GFX::VBOlayout layout;
    layout.Push<float>(3);
    layout.Push<float>(3);
    layout.Push<float>(2);
    blockHoverVao->AddBuffer(*blockHoverVbo, layout);
  }
  blockHoverVao->Bind();
  glDrawArrays(GL_TRIANGLES, 0, 36);
}

void Renderer::DrawSkybox()
{
  auto& shdr = Shader::shaders["skybox"];
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  shdr->Use();
  shdr->setMat4("u_proj", CameraSystem::GetProj());
  shdr->setMat4("u_modview", glm::translate(CameraSystem::GetView(), CameraSystem::GetPos()));
  shdr->setInt("u_skybox", 0);
  CameraSystem::ActiveCamera->skybox->Bind(0);
  emptyVao->Bind();
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 14);
  glDepthMask(GL_TRUE);
  glEnable(GL_CULL_FACE);
}

#include <imgui/imgui.h>
void Renderer::StartFrame()
{
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glClearDepth(0.0f);
  auto cc = glm::vec3(.529f, .808f, .922f);
  glClearColor(cc.r, cc.g, cc.b, 1.f);
  glDepthMask(GL_TRUE);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_GEQUAL);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  ImGui::Begin("Tonemapping");
  ImGui::Checkbox("Enabled", &tonemapping);
  ImGui::Checkbox("Gamma Correction", &gammaCorrection);
  ImGui::SliderFloat("Exposure Factor", &exposure, .5f, 2.0f, "%.2f");
  ImGui::SliderFloat("Min Exposure", &minExposure, .1f, 10.0f, "%.2f");
  ImGui::SliderFloat("Max Exposure", &maxExposure, .1f, 10.0f, "%.2f");
  ImGui::SliderFloat("Target Luminance", &targetLuminance, .1f, 10.0f, "%.2f", 2.f);
  ImGui::SliderFloat("Adjustment Speed", &adjustmentSpeed, .1f, 10.0f, "%.2f");
  if (ImGui::Button("Recompile"))
  {
    Shader::shaders["tonemap"].emplace(Shader(
      {
        { "fullscreen_tri.vs", GL_VERTEX_SHADER },
        { "tonemap.fs", GL_FRAGMENT_SHADER }
      }));
    Shader::shaders["calc_exposure"].emplace(Shader(
      { { "calc_exposure.cs", GL_COMPUTE_SHADER } }));
  }
  ImGui::End();
}

void Renderer::EndFrame(float dt)
{
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if (gammaCorrection)
  {
    glEnable(GL_FRAMEBUFFER_SRGB);
  }

  if (tonemapping)
  {
    glBindTextureUnit(1, color);
    glGenerateTextureMipmap(color);
    exposureBuffer->Bind<GFX::Target::SSBO>(0);
    auto& cshdr = Shader::shaders["calc_exposure"];
    cshdr->Use();
    cshdr->setFloat("u_targetLuminance", targetLuminance);
    cshdr->setFloat("u_minExposure", minExposure);
    cshdr->setFloat("u_maxExposure", maxExposure);
    cshdr->setFloat("u_dt", dt);
    cshdr->setFloat("u_adjustmentSpeed", adjustmentSpeed);
    cshdr->setInt("u_hdrBuffer", 1);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glViewport(0, 0, fboWidth, fboHeight);
    auto& shdr = Shader::shaders["tonemap"];
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    shdr->Use();
    shdr->setFloat("u_exposureFactor", exposure);
    shdr->setInt("u_hdrBuffer", 1);
    emptyVao->Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
  }
  else
  {
    glBlitNamedFramebuffer(fbo, 0,
      0, 0, fboWidth, fboHeight,
      0, 0, windowWidth, windowHeight,
      GL_COLOR_BUFFER_BIT, GL_LINEAR);
  }
  glDisable(GL_FRAMEBUFFER_SRGB);
}
