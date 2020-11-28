//@group UI

#include "UI_Internal.h"

#include "core_Assert.h"
#include "Material.h"
#include "ShaderUniform.h"
#include "RendererProgram.h"
#include "VertexArray.h"
#include "Renderer.h"
#include <algorithm>

namespace Engine
{
  enum UIMesh
  {
    uiM_Box,
    uiM_RoundedBox,

    uiM_COUNT
  };

  static char const * g_box_vs = R"(
      #version 430
      layout(location = 0) in vec2 inPos;
      uniform vec2 windowSize;
      uniform vec2 buttonPos;
      uniform vec2 buttonSize;
      void main()
      {
        vec2 xy = ((inPos * buttonSize + buttonPos)  / windowSize  - vec2(0.5, 0.5)) * 2.0;
        xy.y = -xy.y;
        gl_Position = vec4(xy, 0.0, 1.0);
      })";

  static char const * g_box_fs = R"(
      #version 430
      uniform vec4 colour;
      out vec4 FragColour;
      void main()
      {
        FragColour = colour;
      })";

  static float const g_boxVerts[] =
  {
    0.0f, 0.0f,
    0.0f, 1.0f,
    1.0f, 1.0f,
    1.0f, 0.0f,
  };

  static int const g_boxIndices[] ={1, 2, 3, 3, 0, 1};

  struct Renderable
  {
    Ref<Material>     material;
    Ref<VertexBuffer> vb;
    Ref<IndexBuffer>  ib;
    Ref<VertexArray>  va;
  };

  class UIRenderer::PIMPL
  {
  public:

    Renderable renBox;

    void InitBox();
  };

  UIRenderer * UIRenderer::s_pInstance = nullptr;

  void UIRenderer::PIMPL::InitBox()
  {
    renBox.vb = VertexBuffer::Create(g_boxVerts, SIZEOF32(g_boxVerts));
    renBox.vb->SetLayout(
      {
        { Engine::ShaderDataType::VEC2, "inPos" }
      });

    renBox.ib = Engine::IndexBuffer::Create(g_boxIndices, SIZEOF32(g_boxIndices));
    renBox.va = Engine::VertexArray::Create();

    renBox.va->AddVertexBuffer(renBox.vb);
    renBox.va->SetIndexBuffer(renBox.ib);

    Engine::ShaderData * pSD = new ShaderData({
        { Engine::ShaderDomain::Vertex, Engine::StrType::Source, g_box_vs },
        { Engine::ShaderDomain::Fragment, Engine::StrType::Source, g_box_fs }
      });

    ResourceManager::Instance()->RegisterResource(ir_GUIShaderData, pSD);
    Ref<Engine::RendererProgram> refProg;
    refProg = Engine::RendererProgram::Create(ir_GUIShaderData);
    renBox.material = Material::Create(refProg);
  }

  bool UIRenderer::Init()
  {
    BSR_ASSERT(s_pInstance == nullptr, "UIRenderer already initialised!");
    s_pInstance = new UIRenderer();
    return true;
  }

  void UIRenderer::Destroy()
  {
    delete s_pInstance;
    s_pInstance = nullptr;
  }

  UIRenderer * UIRenderer::Instance()
  {
    BSR_ASSERT(s_pInstance != nullptr);
    return s_pInstance;
  }

  UIRenderer::UIRenderer()
    : m_pimpl(new PIMPL())
  {
    m_pimpl->InitBox();
  }

  UIRenderer::~UIRenderer()
  {
    delete m_pimpl;
  }

  void UIRenderer::SetScreenSize(vec2 const & a_size)
  {
    m_pimpl->renBox.material->SetUniform("windowSize", a_size.GetData(), sizeof(a_size));
  }

  void UIRenderer::DrawBox(UIAABB const & a_aabb, Colour a_colour)
  {
    float clr[4] = {a_colour.fr(), a_colour.fg(), a_colour.fb(), a_colour.fa()};

    m_pimpl->renBox.material->SetUniform("colour", clr, sizeof(clr));
    m_pimpl->renBox.material->SetUniform("buttonPos", a_aabb.position.GetData(), sizeof(a_aabb.position));
    m_pimpl->renBox.material->SetUniform("buttonSize", a_aabb.size.GetData(), sizeof(a_aabb.size));

    m_pimpl->renBox.material->Bind();
    m_pimpl->renBox.va->Bind();

    Renderer::DrawIndexed(6, false);
  }

  /*void UIRenderer::DrawCorner(vec2 const & a_position, vec2 const & a_size, Colour a_colour)
  {
    float clr[4] ={a_colour.fr(), a_colour.fg(), a_colour.fb(), a_colour.fa()};

    m_pimpl->renBox.material->SetUniform("colour", clr, sizeof(clr));
    m_pimpl->renBox.material->SetUniform("buttonPos", a_position.GetData(), sizeof(a_position));
    m_pimpl->renBox.material->SetUniform("buttonSize", a_size.GetData(), sizeof(a_size));

    m_pimpl->renBox.material->Bind();
    m_pimpl->renBox.va->Bind();

    Renderer::DrawIndexed(3, false);
  }

  void UIRenderer::DrawRoundedBox(vec2 const & a_position, vec2 const & a_size, Colour a_colour, float a_radius)
  {
  
  }*/

  bool UIIntersection(UIAABB const & A,
                      UIAABB const & B,
                      UIAABB & out)
  {
    float xMin = std::max(A.position.x(), B.position.x());
    float xMax = std::min(A.position.x() + A.size.x(), B.position.x() + B.size.x());

    if (xMin >= xMax)
      return false;

    float yMin = std::max(A.position.y(), B.position.y());
    float yMax = std::min(A.position.y() + A.size.y(), B.position.y() + B.size.y());

    if (yMin >= yMax)
      return false;

    out.position = vec2(xMin, yMin);
    out.size = vec2(xMax - xMin, yMax - yMin);

    return true;
  }
}