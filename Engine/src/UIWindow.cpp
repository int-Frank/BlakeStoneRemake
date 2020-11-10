//@group UI

#include "UIWindow.h"
#include "UIWidget.h"
#include "UICommon.h"

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
      UIWidget * pParent;
      vec2 position;
      vec2 size;
      Dg::DoublyLinkedList<UIWidget *> children;
    };

    UIWindowState(Data * a_pData);
    virtual ~UIWindowState();

    void Destroy();

    virtual UIState QueryState() const = 0;
    UIWidget * GetParent() const;
    void SetParent(UIWidget * a_pParent);

    vec2 GetPosition() const;
    vec2 GetSize() const;
    void Clear();

    void Add(UIWidget * a_pWgt);
    void Remove(UIWidget * a_pWgt);

    void Draw();

    virtual UIWindowState * HandleMessage(Message *) = 0;

  protected:

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

  //class ResizeState : public UIWindowState
  //{
  //public:
  //
  //  ~ResizeState();
  //
  //  vec2 GetPosition() const;
  //  vec2 GetSize() const;
  //
  //  UIWindowState * HandleMessage(Message *) override;
  //  void HandleMessage(Message_GUI_PointerDown *);
  //  void HandleMessage(Message_GUI_PointerMove *);
  //  void HandleMessage(Message_GUI_Select *);
  //};

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

  vec2 UIWindowState::GetPosition() const
  {
    return m_pData->position;
  }

  vec2 UIWindowState::GetSize() const
  {
    return m_pData->size;
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
    vec2 point((float)a_pMsg->x, (float)a_pMsg->y);
    if (UIPointInBox(m_pData->position, m_pData->size, point))
      return new MoveState(m_pData, point);
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
    , m_positionAnchor(a_pData->position)
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
      return new StaticState(m_pData);

    if (a_pMsg->GetID() == Message_GUI_PointerMove::GetStaticID())
      return HandleMessage(dynamic_cast<Message_GUI_PointerMove *>(a_pMsg));

    return nullptr;
  }

  UIWindowState * MoveState::HandleMessage(Message_GUI_PointerMove * a_pMsg)
  {
    vec2 point((float)a_pMsg->x, (float)a_pMsg->y);
    m_pData->position = m_positionAnchor + (point - m_controlAnchor);
    return nullptr;
  }

  //------------------------------------------------------------------------------------
  // UIWindow
  //------------------------------------------------------------------------------------

  UIWindow::UIWindow(UIWidget * a_pParent, vec2 const a_position, vec2 const & a_size)
    : m_pState(nullptr)
  {
    UIWindowState::Data * pData = new UIWindowState::Data();
    pData->pParent = a_pParent;
    pData->position = a_position;
    pData->size = a_size;
  }

  UIWindow::~UIWindow()
  {
    m_pState->Destroy();
    delete m_pState;
  }

  void UIWindow::HandleMessage(Message * a_pMsg)
  {
    m_pState->HandleMessage(a_pMsg);
  }

  void UIWindow::Clear()
  {
    m_pState->Clear();
  }

  void UIWindow::Add(UIWidget * a_pMsg)
  {
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
}