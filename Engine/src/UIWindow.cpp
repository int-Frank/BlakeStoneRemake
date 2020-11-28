//@group UI

#include "UIWindow.h"
#include "UIWidget.h"
#include "UICommon.h"
#include "UI_Internal.h"
#include "UIButton.h"
#include "Renderer.h"

namespace Engine
{
  //------------------------------------------------------------------------------------
  // State class declarations
  //------------------------------------------------------------------------------------
  class UIWindowState
  {
  public:

    struct Data
    {
      Colour clrBackground;

      UIWindow * pWindow;
      UIWidget * pParent;
      UIAABB aabb;
      UIButton * pGrab;
      bool grabPressed;
      bool grabHover;
      Dg::DoublyLinkedList<UIWidget *> children;
      uint32_t flags;
    };

    UIWindowState(Data * a_pData);
    virtual ~UIWindowState();

    void Destroy();

    virtual UIState QueryState() const = 0;
    UIWidget * GetParent() const;
    void SetParent(UIWidget * a_pParent);

    void Clear();

    void SetPosition(vec2 const &);
    void SetSize(vec2 const &);

    void Add(UIWidget * a_pWgt);
    void Remove(UIWidget * a_pWgt);

    void Draw();
    vec2 GetLocalPosition() const;
    vec2 GetSize() const;

    virtual UIWindowState * HandleMessage(Message *) = 0;

  protected:

    bool HasFlag(UIWindow::Flag) const;

    Data * m_pData;
  };

  class StaticState : public UIWindowState
  {
  public:

    StaticState(Data * a_pData);
    ~StaticState();

    UIState QueryState() const override;

    UIWindowState * HandleMessage(Message * a_pMsg) override;
    UIWindowState * HandleMessage(Message_GUI_PointerDown * a_pMsg);
    UIWindowState * HandleMessage(Message_GUI_PointerUp * a_pMsg);
    UIWindowState * HandleMessage(Message_GUI_PointerMove * a_pMsg);

  private:

    UIWidget * m_pFocus;
    UIState m_state;
  };

  class MoveState : public UIWindowState
  {
  public:

    MoveState(Data * a_pData, vec2 const & a_controlAnchor);
    ~MoveState();

    UIState QueryState() const override;

    UIWindowState * HandleMessage(Message * a_pMsg) override;
    UIWindowState * HandleMessage(Message_GUI_PointerMove * a_pMsg);

  private:

    vec2 m_controlAnchor;
    vec2 m_positionAnchor;
  };

  class ResizeState : public UIWindowState
  {
  public:

    ResizeState(Data * a_pData, vec2 const & a_controlAnchor);
    ~ResizeState();
  
    UIState QueryState() const override;
  
    UIWindowState * HandleMessage(Message *) override;
    UIWindowState * HandleMessage(Message_GUI_PointerMove *);

  private:

    vec2 m_controlAnchor;
    vec2 m_sizeAnchor;
  };

  //------------------------------------------------------------------------------------
  // UIWindowState
  //------------------------------------------------------------------------------------

  UIWindowState::UIWindowState(Data * a_pData)
    : m_pData(a_pData)
  {
    
  }

  UIWindowState::~UIWindowState()
  {
    
  }

  void UIWindowState::Destroy()
  {
    Clear();

    delete m_pData->pGrab;
    delete m_pData;
    m_pData = nullptr;
  }

  UIWidget * UIWindowState::GetParent() const
  {
    return m_pData->pParent;
  }

  void UIWindowState::SetParent(UIWidget * a_pParent)
  {
    m_pData->pParent = a_pParent;
  }

  vec2 UIWindowState::GetSize() const
  {
    return m_pData->aabb.size;
  }

  void UIWindowState::Clear()
  {
    for (auto pWgt : m_pData->children)
      delete pWgt;
    m_pData->children.clear();
  }

  void UIWindowState::Add(UIWidget * a_pWgt)
  {
    m_pData->children.push_back(a_pWgt);
  }

  void UIWindowState::Remove(UIWidget * a_pWgt)
  {
    for (auto it = m_pData->children.begin(); it != m_pData->children.end(); it++)
    {
      if (*it == a_pWgt)
      {
        delete *it;
        m_pData->children.erase(it);
        break;
      }
    }
  }

  void UIWindowState::Draw()
  {
    UIAABB aabb;
    UIAABBType t = m_pData->pWindow->GetGlobalAABB(aabb);
    if (t == UIAABBType::None)
      return;

    if (t == UIAABBType::Window)
    {
      Renderer::Enable(RenderFeature::Sissor);
      Renderer::SetSissorBox((int)aabb.position.x(), (int)aabb.position.y(), (int)aabb.size.x(), (int)aabb.size.y());
      UIRenderer::Instance()->DrawBox({m_pData->pWindow->GetGlobalPosition(), m_pData->pWindow->GetSize()}, m_pData->clrBackground);
      Renderer::Disable(RenderFeature::Sissor);
    }
    else
    {
      UIRenderer::Instance()->DrawBox(m_pData->aabb, m_pData->clrBackground);
    }

    for (UIWidget * pWgt : m_pData->children)
      pWgt->Draw();

    if (m_pData->pGrab != nullptr)
      m_pData->pGrab->Draw();
  }

  bool UIWindowState::HasFlag(UIWindow::Flag a_flag) const
  {
    return (m_pData->flags & a_flag) != 0;
  }

  vec2 UIWindowState::GetLocalPosition() const
  {
    return m_pData->aabb.position;
  }

  void UIWindowState::SetPosition(vec2 const & a_position)
  {
    m_pData->aabb.position = a_position;
  }

  void UIWindowState::SetSize(vec2 const & a_size)
  {
    m_pData->aabb.size = a_size;
  }

  //------------------------------------------------------------------------------------
  // StaticState
  //------------------------------------------------------------------------------------

  StaticState::StaticState(Data * a_pData)
    : UIWindowState(a_pData)
    , m_pFocus(nullptr)
    , m_state(UIState::None)
  {
    
  }

  StaticState::~StaticState()
  {
    
  }

  UIState StaticState::QueryState() const
  {
    return m_state;
  }

  UIWindowState * StaticState::HandleMessage(Message * a_pMsg)
  {
    if (a_pMsg->GetCategory() != MC_GUI)
      return nullptr;

    UIWindowState * pResult = nullptr;

    if (m_pFocus != nullptr)
    {
      m_pFocus->HandleMessage(a_pMsg);
      if (m_pFocus->QueryState() == UIState::None)
      {
        m_pFocus = nullptr;
        m_state = UIState::None;
      }
      if (a_pMsg->QueryFlag(Engine::Message::Flag::Handled))
        pResult = nullptr;
    }
    else if (a_pMsg->GetID() == Message_GUI_PointerDown::GetStaticID())
    {
      pResult = HandleMessage(dynamic_cast<Message_GUI_PointerDown *>(a_pMsg));
    }
    else if (a_pMsg->GetID() == Message_GUI_PointerUp::GetStaticID())
    {
      pResult = HandleMessage(dynamic_cast<Message_GUI_PointerUp *>(a_pMsg));
    }
    else if (a_pMsg->GetID() == Message_GUI_PointerMove::GetStaticID())
    {
      pResult = HandleMessage(dynamic_cast<Message_GUI_PointerMove *>(a_pMsg));
    }
    return pResult;
  }

  UIWindowState * StaticState::HandleMessage(Message_GUI_PointerDown * a_pMsg)
  {
    UIAABB aabb;
    if (m_pData->pWindow->GetGlobalAABB(aabb) == UIAABBType::None)
      return nullptr;

    vec2 point((float)a_pMsg->x, (float)a_pMsg->y);

    if (!UIPointInBox(point, aabb))
      return nullptr;

    if (m_pData->pGrab != nullptr)
    {
      m_pData->pGrab->HandleMessage((Message*)a_pMsg);
      if (m_pData->grabPressed)
      {
        m_pData->grabPressed = false;
        return new ResizeState(m_pData, point);
      }
    }

    for (UIWidget * pWidget : m_pData->children)
    {
      pWidget->HandleMessage(a_pMsg);
      if (a_pMsg->QueryFlag(Engine::Message::Flag::Handled))
      {
        if (pWidget->QueryState() == UIState::HasFocus)
        {
          m_pFocus = pWidget;
          m_state = UIState::HasFocus;
        }
        return nullptr;
      }
    }

    if (HasFlag(UIWindow::Movable) && UIPointInBox(point, aabb))
    {
      a_pMsg->SetFlag(Message::Flag::Handled, true);
      return new MoveState(m_pData, point);
    }

    return nullptr;
  }

  UIWindowState * StaticState::HandleMessage(Message_GUI_PointerUp * a_pMsg)
  {
    for (UIWidget * pWidget : m_pData->children)
    {
      pWidget->HandleMessage(a_pMsg);
      if (a_pMsg->QueryFlag(Engine::Message::Flag::Handled))
      {
        if (pWidget->QueryState() == UIState::HasFocus)
        {
          m_pFocus = pWidget;
          m_state = UIState::HasFocus;
        }
        break;
      }
    }
    return nullptr;
  }

  UIWindowState * StaticState::HandleMessage(Message_GUI_PointerMove * a_pMsg)
  {
    vec2 point((float)a_pMsg->x, (float)a_pMsg->y);

    if (m_pData->pGrab != nullptr)
    {
      m_pData->pGrab->HandleMessage((Message*)a_pMsg);
      if (m_pData->grabHover)
      {
        // TODO come up with a better solution for this. The grab has 'consumed' Message_GUI_PointerMove,
        //      but we still need to pass it on for other UI elements, but with no possibility to interact with 
        //      another element. We are sort of in a half consumed state.
        a_pMsg->x = -1;
        a_pMsg->y = -1;
      }
    }

    for (UIWidget * pWidget : m_pData->children)
    {
      pWidget->HandleMessage(a_pMsg);
      if (a_pMsg->QueryFlag(Engine::Message::Flag::Handled))
      {
        if (pWidget->QueryState() == UIState::HasFocus)
        {
          m_pFocus = pWidget;
          m_state = UIState::HasFocus;
        }
        break;
      }
    }

    return nullptr;
  }

  //------------------------------------------------------------------------------------
  // MoveState
  //------------------------------------------------------------------------------------

  MoveState::MoveState(Data * a_pData, vec2 const & a_controlAnchor)
    : UIWindowState(a_pData)
    , m_controlAnchor(a_controlAnchor)
    , m_positionAnchor(a_pData->aabb.position)
  {
    
  }

  MoveState::~MoveState()
  {
    
  }

  UIState MoveState::QueryState() const
  {
    return UIState::HasFocus;
  }

  UIWindowState * MoveState::HandleMessage(Message * a_pMsg)
  {
    if (a_pMsg->GetCategory() != MC_GUI)
      return nullptr;

    if (a_pMsg->GetID() == Message_GUI_PointerUp::GetStaticID())
    {
      a_pMsg->SetFlag(Message::Flag::Handled, true);
      return new StaticState(m_pData);
    }

    if (a_pMsg->GetID() == Message_GUI_PointerMove::GetStaticID())
      return HandleMessage(dynamic_cast<Message_GUI_PointerMove *>(a_pMsg));

    return nullptr;
  }

  UIWindowState * MoveState::HandleMessage(Message_GUI_PointerMove * a_pMsg)
  {
    vec2 point((float)a_pMsg->x, (float)a_pMsg->y);
    m_pData->aabb.position = m_positionAnchor + (point - m_controlAnchor);
    a_pMsg->SetFlag(Message::Flag::Handled, true);
    return nullptr;
  }

  //------------------------------------------------------------------------------------
  // ResizeState
  //------------------------------------------------------------------------------------

  ResizeState::ResizeState(Data * a_pData, vec2 const & a_controlAnchor)
    : UIWindowState(a_pData)
    , m_controlAnchor(a_controlAnchor)
    , m_sizeAnchor(a_pData->aabb.size)
  {

  }

  ResizeState::~ResizeState()
  {

  }

  UIState ResizeState::QueryState() const
  {
    return UIState::HasFocus;
  }

  UIWindowState * ResizeState::HandleMessage(Message * a_pMsg)
  {
    if (a_pMsg->GetCategory() != MC_GUI)
      return nullptr;

    if (a_pMsg->GetID() == Message_GUI_PointerUp::GetStaticID())
    {
      a_pMsg->SetFlag(Message::Flag::Handled, true);
      return new StaticState(m_pData);
    }

    if (a_pMsg->GetID() == Message_GUI_PointerMove::GetStaticID())
      return HandleMessage(dynamic_cast<Message_GUI_PointerMove *>(a_pMsg));

    return nullptr;
  }

  UIWindowState * ResizeState::HandleMessage(Message_GUI_PointerMove * a_pMsg)
  {
    vec2 point((float)a_pMsg->x, (float)a_pMsg->y);
    vec2 newSize = m_sizeAnchor + (point - m_controlAnchor);

    if (newSize.x() < UIWindow::s_minSize.x())
      newSize.x() = UIWindow::s_minSize.x();

    if (newSize.y() < UIWindow::s_minSize.y())
      newSize.y() = UIWindow::s_minSize.y();

    m_pData->aabb.size = newSize;
    a_pMsg->SetFlag(Message::Flag::Handled, true);

    if (m_pData->pGrab != nullptr)
      m_pData->pGrab->SetPosition(m_pData->aabb.size - m_pData->pGrab->GetSize());

    return nullptr;
  }

  //------------------------------------------------------------------------------------
  // UIWindow
  //------------------------------------------------------------------------------------

  vec2 const UIWindow::s_minSize = vec2(50.f, 20.f);

  UIWindow::UIWindow(UIWidget * a_pParent, vec2 const a_position, vec2 const & a_size, uint32_t a_flags)
    : m_pState(nullptr)
  {
    UIWindowState::Data * pData = new UIWindowState::Data();
    pData->clrBackground = 0xBB000000;
    pData->pParent = a_pParent;
    pData->pWindow = this;
    pData->aabb = {a_position, a_size};
    pData->flags = a_flags;

    vec2 grabSize(16.f, 16.f);
    pData->grabPressed = false;
    pData->pGrab = nullptr;

    if (a_flags & Flag::Resizable)
    {
      pData->pGrab = UIButton::Create(this, "", 0, a_size - grabSize, grabSize);
      pData->pGrab->BindSelect([pBool = &pData->grabPressed](){*pBool = true; });
      pData->pGrab->BindHoverOn([pBool = &pData->grabHover](){*pBool = true; });
      pData->pGrab->BindHoverOff([pBool = &pData->grabHover](){*pBool = false; });
      pData->pGrab->SetBackgroundColour(Colour(43, 145, 175, 255));
      pData->pGrab->SetHoverOnBackgroundColour(Colour(63, 195, 225, 255));
    }

    m_pState = new StaticState(pData);
  }

  UIWindow::~UIWindow()
  {
    m_pState->Destroy();
    delete m_pState;
  }

  UIWindow * UIWindow::Create(UIWidget * a_pParent, vec2 const a_position, vec2 const & a_size, uint32_t a_flags)
  {
    return new UIWindow(a_pParent, a_position, a_size, a_flags);
  }

  void UIWindow::HandleMessage(Message * a_pMsg)
  {
    UpdateState(m_pState->HandleMessage(a_pMsg));
  }

  void UIWindow::Clear()
  {
    m_pState->Clear();
  }

  void UIWindow::Add(UIWidget * a_pMsg)
  {
    a_pMsg->SetParent(this);
    m_pState->Add(a_pMsg);
  }

  void UIWindow::Remove(UIWidget * a_pMsg)
  {
    m_pState->Remove(a_pMsg);
  }

  void UIWindow::Draw()
  {
    m_pState->Draw();
  }

  UIState UIWindow::QueryState() const
  {
    return m_pState->QueryState();
  }

  UIWidget * UIWindow::GetParent() const
  {
    return m_pState->GetParent();
  }
  
  void UIWindow::SetParent(UIWidget * a_pParent)
  {
    m_pState->SetParent(a_pParent);
  }

  void UIWindow::UpdateState(UIWindowState * a_pState)
  {
    if (a_pState != nullptr)
    {
      delete m_pState;
      m_pState = a_pState;
    }
  }

  vec2 UIWindow::GetLocalPosition() const
  {
    return m_pState->GetLocalPosition();
  }

  vec2 UIWindow::GetSize() const
  {
    return m_pState->GetSize();
  }

  void UIWindow::SetPosition(vec2 const & a_position)
  {
    m_pState->SetPosition(a_position);
  }

  void UIWindow::SetSize(vec2 const & a_size)
  {
    m_pState->SetSize(a_size);
  }
}