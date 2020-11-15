
#include <chrono>
#include <map>

#include "Engine.h"
#include "System_Input.h"
#include "Renderer.h"
#include "RT_RendererAPI.h"
#include "Buffer.h"
#include "VertexArray.h"
#include "RendererProgram.h"
#include "Material.h"
#include "Texture.h"
#include "InputCodes.h"

#include "UIButton.h"
#include "UIWindow.h"
#include "EngineMessages.h"
#include "DgBinPacker.h"
#include "DgRNG_Local.h"

#define TEXTURE_XY 512

Colour * GenerateBinTexture()
{
  int itemMin = 8;
  int itemMax = 64;

  int nItems = 220;

  Dg::RNG_Local rng;
  rng.SetSeed(14);

  struct MyItem
  {
    int dim[2];
    int pos[2];
  };

  Dg::BinPacker<int> rp;
  std::map<Dg::BinPackerCommon::ItemID, MyItem> itemMap;

  for (int i = 0; i < nItems; i++)
  {
    MyItem item;
    item.dim[Dg::Element::width] = rng.GetUintRange(itemMin, itemMax);
    item.dim[Dg::Element::height] = rng.GetUintRange(itemMin, itemMax);

    Dg::BinPackerCommon::ItemID id = rp.RegisterItem(item.dim[Dg::Element::width], item.dim[Dg::Element::height]);
    itemMap.insert(std::pair<Dg::BinPackerCommon::ItemID, MyItem>(id, item));
  }

  Dg::BinPacker<int>::Bin bin;
  bin.dimensions[0] = itemMax;
  bin.dimensions[1] = itemMax;
  bin.maxDimensions[0] = TEXTURE_XY;
  bin.maxDimensions[1] = TEXTURE_XY;

  size_t leftovers = rp.Fill(bin);
  //bin.items.clear();
  //leftovers = rp.Fill(bin);

  LOG_INFO("Leftovers: {}", leftovers);

  Dg::DynamicArray<MyItem> items;

  for (auto const & item : bin.items)
  {
    Dg::BinPackerCommon::ItemID id = item.id;
    itemMap.at(id).pos[Dg::Element::x] = item.xy[Dg::Element::x];
    itemMap.at(id).pos[Dg::Element::y] = item.xy[Dg::Element::y];
    items.push_back(itemMap.at(id));
  }

  LOG_INFO("Item count: {}", items.size());

  Colour * pPixels = new Colour[TEXTURE_XY * TEXTURE_XY];

  for (int i = 0; i < TEXTURE_XY * TEXTURE_XY; i++)
  {
    pPixels[i].a(255);
    pPixels[i].r(0);
    pPixels[i].g(0);
    pPixels[i].b(0);
  }

  for (auto const & item : items)
  {
    int overlap = 0;
    for (auto item2 : items)
    {
      if (item.pos[0] == item2.pos[0] && item.pos[1] == item2.pos[1])
        overlap++;
    }

    if (overlap > 1)
    {
      LOG_INFO("FOUND: [{}, {}], {}", item.pos[0], item.pos[1], overlap);
    }
    rng.SetSeed(234);
    uint32_t r = rng.GetUintRange(128, 255);
    uint32_t g = rng.GetUintRange(128, 255);
    uint32_t b = rng.GetUintRange(128, 255);

    for (int x = item.pos[Dg::Element::x]; x < item.pos[Dg::Element::x] + item.dim[Dg::Element::width]; x++)
    {
      for (int y = item.pos[Dg::Element::y]; y < item.pos[Dg::Element::y] + item.dim[Dg::Element::height]; y++)
      {
        pPixels[x + y * TEXTURE_XY].a(255);
        pPixels[x + y * TEXTURE_XY].r(r);
        pPixels[x + y * TEXTURE_XY].g(g);
        pPixels[x + y * TEXTURE_XY].b(b);
      }
    }
  }
  return pPixels;
}

Colour * GenerateTexture(uint32_t a_width, uint32_t a_height)
{
  Colour * pixels = new Colour[a_width * a_height];

  for (uint32_t y = 0; y < a_height; y++)
  {
    for (uint32_t x = 0; x < a_width; x++)
    {
      uint32_t ind = y * a_width + x;
      pixels[ind].a(255);
      pixels[ind].r(y < a_height / 2 ? 255 : 0);
      pixels[ind].g(x < a_width / 2 ? 255 : 0);
      pixels[ind].b(0);
    }
  }

  return pixels;
}

class GameSystem : public Engine::System
{
public:

  ASSIGN_ID(256);
  GameSystem()
  {

  }

  void OnAttach() override
  {
    Engine::Renderer::SetClearColor(1.0f, 0.0f, 1.0f);

    float verts[] = 
    {
      -1.0f, 1.0f, 0.0f, 0.0f,
      0.5f, 1.0f,  1.0, 0.0,
      0.5f, -0.66666f,  1.0, 1.0,
      -1.0f, -0.66666f, 0.0, 1.0
    };

    int indices[] = {0, 1, 2, 0, 2, 3};

    m_vb = Engine::VertexBuffer::Create(verts, SIZEOF32(verts));
    m_vb->SetLayout(
      {
        { Engine::ShaderDataType::VEC2, "inPos" }, //TODO make use of these strings. Maybe verify with rendererProgram?
        { Engine::ShaderDataType::VEC2, "inTexCoord" },
      });

    m_ib = Engine::IndexBuffer::Create(indices, SIZEOF32(indices));
    m_va = Engine::VertexArray::Create();

    m_va->AddVertexBuffer(m_vb);
    m_va->SetIndexBuffer(m_ib);

    Engine::ShaderData *pSD = new Engine::ShaderData({
        { Engine::ShaderDomain::Vertex, Engine::StrType::Path, "src/vs.glsl" },
        { Engine::ShaderDomain::Fragment, Engine::StrType::Path, "src/fs.glsl" }
      });

    Engine::ResourceID sdID = 12345;
    Engine::ResourceManager::Instance()->RegisterResource(sdID, pSD);

    Engine::Ref<Engine::RendererProgram> refProg;
    refProg = Engine::RendererProgram::Create(sdID);

    Engine::TextureFlags flags;
    flags.SetFilter(Engine::TextureFilter::Linear);
    flags.SetIsMipmapped(false);
    flags.SetWrap(Engine::TextureWrap::Clamp);
    m_texture = Engine::Texture2D::Create();
    m_texture->Set(TEXTURE_XY, TEXTURE_XY, GenerateBinTexture(), flags);
    m_texture->Upload();

    m_material = Engine::Material::Create(refProg);

    m_material->SetTexture("texture1", m_texture);
    m_material->Bind();

    m_pButton = new Engine::UIButton(nullptr, "Hello", 0, {20.f, 20.f}, {200.f, 200.f});
    m_pButton->BindHoverSelect([]() {LOG_WARN("PRESSED");});
    m_pButton->BindHoverOn([](){LOG_DEBUG("HOVER ON");});
    m_pButton->BindHoverOff([](){LOG_DEBUG("HOVER OFF");});

  }

  void HandleMessage(Engine::Message* a_pMsg) override
  {
    m_pButton->HandleMessage(a_pMsg);
  }

  void OnDetach() override
  {

  }

  void Update(float a_dt) override
  {
    //std::this_thread::sleep_for(std::chrono::milliseconds(200));
    //LOG_DEBUG("ASDF");
  }

  void Render() override
  {
    Engine::Renderer::Clear(1.0f, 0.0f, 1.0f);

    m_material->Bind();
    m_va->Bind();

    Engine::Renderer::DrawIndexed(6, false);

  }

private:

  Engine::Ref<Engine::VertexBuffer>     m_vb;
  Engine::Ref<Engine::IndexBuffer>      m_ib;
  Engine::Ref<Engine::VertexArray>      m_va;
  Engine::Ref<Engine::Texture2D>        m_texture;
  Engine::Ref<Engine::Material>         m_material;
  Engine::UIButton *                    m_pButton;
  //Engine::UIForm                        m_form;
};

class Game : public Engine::Application
{
public:
  Game(Opts const & a_opts)
    : Application(a_opts)
  {
    PushSystem(new GameSystem());

    Engine::System_Input * layer = static_cast<Engine::System_Input *>(GetSystem(Engine::System_Input::GetStaticID()));
    if (!layer)
      LOG_ERROR("Couldn't find input layer!");
    else
    {
      // Hook up GUI events
      layer->AddBinding(Engine::Message_Input_MouseMove::GetStaticID(),
        [](Engine::Message const * pMsg)
        {
          Engine::Message_Input_MouseMove * pIn = (Engine::Message_Input_MouseMove *)pMsg;
          Engine::Message_GUI_PointerMove * pOut = nullptr;
          ALLOC_NEW_MESSAGE(Engine::Message_GUI_PointerMove, pOut);
          pOut->x = pIn->x;
          pOut->y = pIn->y;
        });

      layer->AddBinding(Engine::Message_Input_MouseButtonDown::GetStaticID(),
        [](Engine::Message const * pMsg)
        {
          Engine::Message_Input_MouseButtonDown * pIn = (Engine::Message_Input_MouseButtonDown *)pMsg;
          if (pIn->button != Engine::IC_MOUSE_BUTTON_LEFT)
            return;

          Engine::Message_GUI_PointerDown * pOut = nullptr;
          ALLOC_NEW_MESSAGE(Engine::Message_GUI_PointerDown, pOut);
          pOut->x = pIn->x;
          pOut->y = pIn->y;
        });

      layer->AddBinding(Engine::Message_Input_MouseButtonUp::GetStaticID(),
        [](Engine::Message const * pMsg)
        {
          Engine::Message_Input_MouseButtonUp * pIn = (Engine::Message_Input_MouseButtonUp *)pMsg;
          if (pIn->button != Engine::IC_MOUSE_BUTTON_LEFT)
            return;

          Engine::Message_GUI_PointerUp * pOut = nullptr;
          ALLOC_NEW_MESSAGE(Engine::Message_GUI_PointerUp, pOut);
          pOut->x = pIn->x;
          pOut->y = pIn->y;
        });
    }

    LOG_TRACE("Game initialised!");
    //RequestQuit();
  }

  ~Game()
  {

  }

};

Engine::Application* Engine::CreateApplication()
{
  Application::Opts opts;
  return new Game(opts);
}
