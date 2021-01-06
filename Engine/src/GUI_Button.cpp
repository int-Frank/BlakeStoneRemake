//@group GUI

#include "GUI_Button.h"
#include "MessageHandler.h"
#include "GUI_Internal.h"
#include "GUI.h"
#include "GUI_Text.h"
#include "Renderer.h"

namespace Engine
{
  namespace GUI
  {

    Button::Button(Widget * a_pParent, std::string const & a_text, vec2 const & a_position, vec2 const & a_size, std::initializer_list<WidgetFlag> flags)
      : Widget(flags)
      , m_pText(nullptr)
      , m_clrDefault(GetStyle().colours[col_Button])
      , m_clrHover(GetStyle().colours[col_ButtonHover])
      , m_clrTextDefault(GetStyle().colours[col_ButtonText])
      , m_clrTextHover(GetStyle().colours[col_ButtonTextHover])
      , m_aabb{a_position, a_size}
      , m_state(WidgetState::None)
      , m_contentBoarder(-4.f)
      , m_pParent(a_pParent)
      , m_clbk_HoverOn(nullptr)
      , m_clbk_HoverOff(nullptr)
      , m_clbk_Select(nullptr)
    {
      TextAttributes attr = {};
      attr.size = GUI_FONT_SIZE;
      attr.colourText = GetStyle().colours[col_ButtonText];
      attr.horizontalAlign = HorizontalAlignment::Centre;
      attr.verticalAlign = VerticalAlignment::Centre;
      attr.lineSpacing = GetStyle().textLineSpacing;
      attr.wrapText = true;

      m_pText = Text::Create(this, a_text, {0.f, 0.f}, a_size, &attr, 
        {WidgetFlag::NotResponsive, WidgetFlag::StretchHeight, WidgetFlag::StretchWidth});
    }

    Button::~Button()
    {
      delete m_pText;
    }

    Button * Button::Create(Widget * pParent, std::string const & text, vec2 const & position, vec2 const & size, std::initializer_list<WidgetFlag> flags)
    {
      return new Button(pParent, text, position, size, flags);
    }

    void Button::BindHoverOn(std::function<void()> a_fn)
    {
      m_clbk_HoverOn = a_fn;
    }

    void Button::BindHoverOff(std::function<void()> a_fn)
    {
      m_clbk_HoverOff = a_fn;
    }

    void Button::BindSelect(std::function<void()> a_fn)
    {
      m_clbk_Select = a_fn;
    }

    void Button::_HandleMessage(Message * a_pMsg)
    {
      if (a_pMsg->GetCategory() != MC_GUI)
        return;

      DISPATCH_MESSAGE(Message_GUI_PointerMove);
      DISPATCH_MESSAGE(Message_GUI_PointerDown);
    }

    void Button::HandleMessage(Message_GUI_PointerDown * a_pMsg)
    {
      UIAABB aabb;
      if (!GetGlobalAABB(aabb))
        return;

      if (PointInBox(vec2((float)a_pMsg->x, (float)a_pMsg->y), aabb) && m_clbk_Select != nullptr)
      {
        m_clbk_Select();
        a_pMsg->SetFlag(Engine::Message::Flag::Handled, true);
      }
    }

    void Button::HandleMessage(Message_GUI_PointerMove * a_pMsg)
    {
      UIAABB aabb;
      if (!GetGlobalAABB(aabb))
        return;

      bool isInside = PointInBox(vec2((float)a_pMsg->x, (float)a_pMsg->y), aabb);
      if (isInside)
        a_pMsg->ConsumeHover();

      if (isInside && m_state == WidgetState::None)
      {
        m_state = WidgetState::HoverOn;
        if (m_clbk_HoverOn != nullptr)
          m_clbk_HoverOn();
      }
      if (!isInside && m_state == WidgetState::HoverOn)
      {
        m_state = WidgetState::None;
        if (m_clbk_HoverOff != nullptr)
          m_clbk_HoverOff();
      }
    }

    void Button::ClearBindings()
    {
      m_clbk_HoverOn = nullptr;
      m_clbk_HoverOff = nullptr;
      m_clbk_Select = nullptr;
    }

    void Button::Draw()
    {
      UIAABB viewableWindow;
      if (!GetGlobalAABB(viewableWindow))
        return;

      ::Engine::Renderer::SetSissorBox((int)viewableWindow.position.x(), (int)viewableWindow.position.y(), (int)viewableWindow.size.x(), (int)viewableWindow.size.y());
      Renderer::DrawBox({GetGlobalPosition(), GetSize()}, m_state == WidgetState::None ? m_clrDefault : m_clrHover);

      m_pText->SetColour(m_state == WidgetState::None ? m_clrTextDefault : m_clrTextHover);
      m_pText->Draw();
    }

    WidgetState Button::QueryState() const
    {
      return m_state;
    }

    Widget * Button::GetParent() const
    {
      return m_pParent;
    }

    void Button::SetParent(Widget * a_pParent)
    {
      m_pParent = a_pParent;
    }

    vec2 Button::_GetLocalPosition()
    {
      return m_aabb.position;
    }

    vec2 Button::_GetSize()
    {
      return m_aabb.size;
    }

    void Button::_SetLocalPosition(vec2 const & a_pos)
    {
      m_aabb.position = a_pos;
    }

    void Button::_SetSize(vec2 const & a_size)
    {
      m_aabb.size = a_size;
    }

    vec2 Button::GetContentDivPosition()
    {
      return vec2(m_contentBoarder, m_contentBoarder);
    }

    vec2 Button::GetContentDivSize()
    {
      vec2 size = GetSize() - vec2(m_contentBoarder * 2.0f, m_contentBoarder * 2.0f);

      if ((size[0] <= 0.0f) || (size[1] <= 0.0f))
        size.Zero();

      return size;
    }

    void Button::SetBackgroundColour(Colour a_clr)
    {
      m_clrDefault = a_clr;
    }

    void Button::SetHoverOnBackgroundColour(Colour a_clr)
    {
      m_clrHover = a_clr;
    }

    void Button::SetTextColour(Colour a_clr)
    {
      m_clrTextDefault = a_clr;
    }

    void Button::SetHoverOnTextColour(Colour a_clr)
    {
      m_clrTextHover = a_clr;
    }
  }
}