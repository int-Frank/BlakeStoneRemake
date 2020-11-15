//@group Core

#include <exception>

#include "MessageBus.h"
#include "SystemStack.h"
#include "Framework.h"
#include "RenderThread.h"

#include "core_ErrorCodes.h"
#include "Application.h"
#include "Options.h"
#include "core_Log.h"
#include "IWindow.h"
#include "core_Assert.h"
#include "Message.h"
#include "Memory.h"
#include "Renderer.h"
#include "ResourceManager.h"

#include "System_Console.h"
#include "System_Input.h"
#include "System_Window.h"
#include "System_UI.h"
#include "System_Application.h"

namespace Engine
{
  class Application::PIMPL
  {
  public:

    PIMPL()
      : pWindow(nullptr)
      , shouldQuit(false)
    {
    
    }

    bool        shouldQuit;
    IWindow *   pWindow;
    SystemStack  layerStack;
  };

  //------------------------------------------------------------------------------------
  // Application...
  //------------------------------------------------------------------------------------
  Application * Application::s_instance = nullptr;

  void Application::InitWindow()
  {
    m_pimpl->pWindow = Framework::Instance()->GetWindow();
    if (m_pimpl->pWindow == nullptr)
      throw std::runtime_error("GetWindow() has returned a null pointer!");

    if (m_pimpl->pWindow->Init() != Core::EC_None)
      throw std::runtime_error("Failed to initialise window!");
  }

  Application * Application::Instance()
  {
    return s_instance;
  }

  void Application::PushLayer(System* a_pLayer)
  {
    m_pimpl->layerStack.PushLayer(a_pLayer, a_pLayer->GetID());
  }

  System * Application::GetLayer(System::ID a_id)
  {
    return m_pimpl->layerStack.GetLayer(a_id);
  }

  Application::Application(Opts const & a_opts)
    : m_pimpl(new PIMPL())
  {
    BSR_ASSERT(s_instance == nullptr, "Error, Application already created!");
    s_instance = this;

    ResourceManager::Init();
    MessageBus::Init(m_pimpl->layerStack);

    if (a_opts.loggerType == E_UseFileLogger)
      Core::impl::Logger::Init_file(a_opts.loggerName.c_str(), a_opts.logFile.c_str());
    else
      Core::impl::Logger::Init_stdout(a_opts.loggerName.c_str());

    if (Framework::Init() != Core::EC_None)
      throw std::runtime_error("Failed to initialise framework!");

    InitWindow();

    if (!Renderer::Init())
      throw std::runtime_error("Failed to initialise Renderer!");

    if (!RenderThread::Init())
      throw std::runtime_error("Failed to initialise Renderer!");

    Framework::ImGui_InitData imguiData;
    m_pimpl->pWindow->GetDimensions(imguiData.window_w, imguiData.window_h);
    //Framework::Instance()->InitImGui(imguiData);

    m_pimpl->layerStack.PushLayer(new System_Application(), System_Application::GetStaticID());
    m_pimpl->layerStack.PushLayer(new System_Input(), System_Input::GetStaticID());
    m_pimpl->layerStack.PushLayer(new System_Window(m_pimpl->pWindow), System_Window::GetStaticID());
    m_pimpl->layerStack.PushLayer(new System_Console(), System_Console::GetStaticID());
    m_pimpl->layerStack.PushLayer(new System_UI(), System_UI::GetStaticID());

    LOG_TRACE("Application initialised!");
  }

  Application::~Application()
  {
    //TODO Before we shut down the render thread, we need to process all
    //     calls to destroy render objects

    RenderThread::ShutDown();
    Renderer::ShutDown();

    if (Framework::ShutDown() != Core::EC_None)
      LOG_ERROR("Failed to shut down framework!");

    s_instance = nullptr;
    MessageBus::ShutDown();
    ResourceManager::ShutDown();

    LOG_TRACE("Shutdown complete!");
  }

  void Application::EndFrame()
  {
    RenderThread::Instance()->Sync();

    RenderState state = RenderState::Create();
    state.Set<RenderState::Attr::Type>(RenderState::Type::Command);
    state.Set<RenderState::Attr::Command>(RenderState::Command::SwapWindow);

    RENDER_SUBMIT(state, []
      {
        Framework::Instance()->GetWindow()->SwapBuffers();
      });

    Renderer::Instance()->SwapBuffers();
    MessageBus::Instance()->SwapBuffers();
    Engine::TBUFClear();
    RenderThread::Instance()->Continue();
  }

  void Application::Run()
  {
    //Start to execute any renderer commands generated on startup
    EndFrame();

    while (!m_pimpl->shouldQuit)
    {
      float dt = 1.0f / 60.0f;

      MessageBus::Instance()->DispatchMessages();

      for (auto it = m_pimpl->layerStack.begin(); it != m_pimpl->layerStack.end(); it++)
        it->second->Update(dt);

      //System_UI * imguiLayer = static_cast<System_UI*>(m_pimpl->layerStack.GetLayer(System_UI::GetID()));
      //imguiLayer->NewFrame();
      //for (auto it = m_pimpl->layerStack.begin(); it != m_pimpl->layerStack.end(); it++)
      //  it->second->DoImGui();

      auto it = m_pimpl->layerStack.end();
      while (it != m_pimpl->layerStack.begin())
      {
        it--;
        it->second->Render();
      }

      EndFrame();
    }
  }

  void Application::RequestQuit()
  {
    m_pimpl->shouldQuit = true;
  }

  bool Application::NormalizeWindowCoords(int a_x, int a_y, float& a_x_out, float& a_y_out)
  {
    if (m_pimpl->pWindow == nullptr)
      return false;
    int w(0), h(0);
    m_pimpl->pWindow->GetDimensions(w, h);
    a_x_out = float(a_x) / float(w);
    a_y_out = float(a_y) / float(h);
    return true;
  }
}
