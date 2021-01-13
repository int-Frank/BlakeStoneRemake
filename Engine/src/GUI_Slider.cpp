//@group GUI

#include "Utils.h"
#include "GUI.h"
#include "GUI_Internal.h"
#include "GUI_Slider.h"
#include "Renderer.h"

#define SLIDER_MIN_RUN_PIXELS 4.0f
#define SLIDER_MIN_CARET_WIDTH 2.0f

namespace Engine
{
  namespace GUI
  {
    //------------------------------------------------------------------------------------
    // State class declarations
    //------------------------------------------------------------------------------------
    class Slider::InternalState
    {
    public:

      struct Data
      {
        float value; // normalised between 0 and 1
        float width;
        float barHeight;
        float caretWidth;
        float caretHeight;
        float outlineWidth;
        vec2 position;
        Colour clr[(int)SliderState::COUNT][(int)SliderElement::COUNT];
        Slider * pSlider;
        Widget * pParent;

        std::function<void()> m_clbk_HoverOn;
        std::function<void()> m_clbk_HoverOff;
      };

      InternalState(Data * a_pData);
      virtual ~InternalState();

      void Destroy();

      void SetColour(SliderState, SliderElement, Colour);
      void BindHoverOn(std::function<void()> a_fn);
      void BindHoverOff(std::function<void()> a_fn);

      virtual void Draw() = 0;
      virtual WidgetState QueryState() const = 0;
      Widget * GetParent() const;
      void SetParent(Widget * a_pParent);

      void SetVal(float);

      virtual InternalState * HandleMessage(Message *) = 0;

      float GetGlobalBarCentre();
      void _SetLocalPosition(vec2 const &);
      void _SetSize(vec2 const &);
      vec2 _GetLocalPosition();
      vec2 _GetSize();

    protected:

      void _Draw(SliderState);
      void GetAABBs(UIAABB & lower, UIAABB & upper, UIAABB & caret);
      void GetAABBs(UIAABB & bar, UIAABB & caret);
      void GetInnerAABBs(UIAABB & lower, UIAABB & upper, UIAABB & caret);
      void SetValFromScreenCoord(float x);

      Data * m_pData;
    };

    class SliderStaticState : public Slider::InternalState
    {
    public:

      SliderStaticState(Data * a_pData, WidgetState);
      ~SliderStaticState();

      void Draw();
      WidgetState QueryState() const override;

      InternalState * HandleMessage(Message * a_pMsg) override;
      InternalState * HandleMessage(Message_GUI_PointerDown * a_pMsg);
      InternalState * HandleMessage(Message_GUI_PointerMove * a_pMsg);

    private:

      WidgetState m_state;
    };

    class SliderActiveState : public Slider::InternalState
    {
    public:

      SliderActiveState(Data * a_pData, float caretOffset);
      ~SliderActiveState();

      void Draw();
      WidgetState QueryState() const override;

      InternalState * HandleMessage(Message * a_pMsg) override;
      InternalState * HandleMessage(Message_GUI_PointerMove * a_pMsg);

    private:

      float m_offset;
    };

    //------------------------------------------------------------------------------------
    // InternalState
    //------------------------------------------------------------------------------------

    Slider::InternalState::InternalState(Data * a_pData)
      : m_pData(a_pData)
    {

    }

    Slider::InternalState::~InternalState()
    {

    }

    void Slider::InternalState::Destroy()
    {
      delete m_pData;
      m_pData = nullptr;
    }

    void Slider::InternalState::BindHoverOn(std::function<void()> a_fn)
    {
      m_pData->m_clbk_HoverOn = a_fn;
    }

    void Slider::InternalState::BindHoverOff(std::function<void()> a_fn)
    {
      m_pData->m_clbk_HoverOff = a_fn;
    }

    Widget * Slider::InternalState::GetParent() const
    {
      return m_pData->pParent;
    }

    void Slider::InternalState::SetParent(Widget * a_pParent)
    {
      m_pData->pParent = a_pParent;
    }

    void Slider::InternalState::SetVal(float a_val)
    {
      if (a_val < 0.0f)
        a_val = 0.0f;
      else if (a_val > 1.0f)
        a_val = 1.0f;

      m_pData->value = a_val;
    }

    vec2 Slider::InternalState::_GetSize()
    {
      float h = m_pData->caretHeight > m_pData->barHeight ? m_pData->caretHeight : m_pData->barHeight;
      return vec2(m_pData->width, h);
    }

    void Slider::InternalState::SetColour(SliderState a_state, SliderElement a_ele, Colour a_clr)
    {
      BSR_ASSERT(a_state != SliderState::COUNT);
      BSR_ASSERT(a_ele != SliderElement::COUNT);
      m_pData->clr[(int)a_state][(int)a_ele] = a_clr;
    }

    void Slider::InternalState::_Draw(SliderState a_state)
    {
      UIAABB viewableWindow;
      if (!m_pData->pSlider->GetGlobalAABB(viewableWindow))
        return;

      ::Engine::Renderer::SetSissorBox((int)viewableWindow.position.x(), (int)viewableWindow.position.y(), (int)viewableWindow.size.x(), (int)viewableWindow.size.y());

      UIAABB lower, upper, caret;
      GetInnerAABBs(lower, upper, caret);

      int s = (int)a_state;

      Renderer::DrawBoxWithOutline(lower, m_pData->outlineWidth, m_pData->clr[s][(int)SliderElement::Lower], m_pData->clr[s][(int)SliderElement::Outline]);
      Renderer::DrawBoxWithOutline(upper, m_pData->outlineWidth, m_pData->clr[s][(int)SliderElement::Upper], m_pData->clr[s][(int)SliderElement::Outline]);
      Renderer::DrawBoxWithOutline(caret, m_pData->outlineWidth, m_pData->clr[s][(int)SliderElement::Caret], m_pData->clr[s][(int)SliderElement::Outline]);
    }

    float Slider::InternalState::GetGlobalBarCentre()
    {
      return m_pData->pSlider->GetGlobalPosition().x() + m_pData->caretWidth / 2.0f + ((m_pData->width - m_pData->caretWidth) * m_pData->value);
    }

    void Slider::InternalState::GetAABBs(UIAABB & a_lower, UIAABB & a_upper, UIAABB & a_caret)
    {
      vec2 size = _GetSize();
      vec2 globalPos = m_pData->pSlider->GetGlobalPosition();
      float barBeginX = globalPos.x();
      float barCentreX = GetGlobalBarCentre();
      float barEndX = globalPos.x() + m_pData->width;
      float barY = globalPos.y() + (size.y() - m_pData->barHeight) / 2.0f;
      float caretX = barBeginX + m_pData->value * (m_pData->width - m_pData->caretWidth);
      float caretY = globalPos.y() + (size.y() - m_pData->caretHeight) / 2.0f;

      a_lower ={vec2(barBeginX, barY), vec2(barCentreX - barBeginX, m_pData->barHeight)};
      a_upper ={vec2(barCentreX, barY), vec2(barEndX - barCentreX, m_pData->barHeight)};
      a_caret ={vec2(caretX, caretY), vec2(m_pData->caretWidth, m_pData->caretHeight)};
    }

    void Slider::InternalState::GetAABBs(UIAABB & a_bar, UIAABB & a_caret)
    {
      vec2 size = _GetSize();
      vec2 globalPos = m_pData->pSlider->GetGlobalPosition();
      float barBeginX = globalPos.x();
      float barY = globalPos.y() + (size.y() - m_pData->barHeight) / 2.0f;
      float caretX = barBeginX + m_pData->value * (m_pData->width - m_pData->caretWidth);
      float caretY = globalPos.y() + (size.y() - m_pData->caretHeight) / 2.0f;

      a_bar = {vec2(barBeginX, barY), vec2(m_pData->width, m_pData->barHeight)};
      a_caret = {vec2(caretX, caretY), vec2(m_pData->caretWidth, m_pData->caretHeight)};
    }

    void Slider::InternalState::GetInnerAABBs(UIAABB & a_lower, UIAABB & a_upper, UIAABB & a_caret)
    {
      GetAABBs(a_lower, a_upper, a_caret);

      vec2 posOffset(m_pData->outlineWidth, m_pData->outlineWidth);
      a_lower.position += posOffset;
      a_upper.position += posOffset;
      a_caret.position += posOffset;

      a_lower.size -= 2.0f * posOffset;
      a_upper.size -= 2.0f * posOffset;
      a_caret.size -= 2.0f * posOffset;
    }

    void Slider::InternalState::SetValFromScreenCoord(float a_x)
    {
      float xPos = m_pData->pSlider->GetGlobalPosition().x() + m_pData->caretWidth / 2.0f;
      m_pData->value = (a_x - xPos) / (m_pData->width - m_pData->caretWidth);
      if (m_pData->value < 0.0f)
        m_pData->value = 0.0f;
      else if (m_pData->value > 1.0f)
        m_pData->value = 1.0f;
    }

    vec2 Slider::InternalState::_GetLocalPosition()
    {
      return m_pData->position;
    }

    void Slider::InternalState::_SetLocalPosition(vec2 const & a_position)
    {
      m_pData->position;
    }

    void Slider::InternalState::_SetSize(vec2 const & a_size)
    {
      m_pData->width = a_size.x();
      if (m_pData->width < (m_pData->caretWidth + SLIDER_MIN_RUN_PIXELS))
        m_pData->width = (m_pData->caretWidth + SLIDER_MIN_RUN_PIXELS);
    }

    //------------------------------------------------------------------------------------
    // SliderStaticState
    //------------------------------------------------------------------------------------

    SliderStaticState::SliderStaticState(Data * a_pData, WidgetState a_state)
      : InternalState(a_pData)
      , m_state(a_state)
    {

    }

    SliderStaticState::~SliderStaticState()
    {

    }

    WidgetState SliderStaticState::QueryState() const
    {
      return m_state;
    }

    void SliderStaticState::Draw()
    {
      _Draw(m_state == WidgetState::HoverOn ? SliderState::Hover : SliderState::Normal);
    }

    Slider::InternalState * SliderStaticState::HandleMessage(Message * a_pMsg)
    {
      if (a_pMsg->GetCategory() != MC_GUI)
        return nullptr;

      InternalState * pResult = nullptr;

      if (a_pMsg->GetID() == Message_GUI_PointerDown::GetStaticID())
        pResult = HandleMessage(dynamic_cast<Message_GUI_PointerDown *>(a_pMsg));
      else if (a_pMsg->GetID() == Message_GUI_PointerMove::GetStaticID())
        pResult = HandleMessage(dynamic_cast<Message_GUI_PointerMove *>(a_pMsg));
      return pResult;
    }

    Slider::InternalState * SliderStaticState::HandleMessage(Message_GUI_PointerDown * a_pMsg)
    {
      UIAABB aabb;
      if (!m_pData->pSlider->GetGlobalAABB(aabb))
        return nullptr;

      vec2 point((float)a_pMsg->x, (float)a_pMsg->y);

      if (!PointInBox(point, aabb))
        return nullptr;

      UIAABB bar, caret;
      GetAABBs(bar, caret);
      if (PointInBox(point, caret))
      {
        a_pMsg->SetFlag(Message::Flag::Handled, true);
        float caretOffset = GetGlobalBarCentre() - point.x();
        return new SliderActiveState(m_pData, caretOffset);
      }
      else if (PointInBox(point, bar))
      {
        a_pMsg->SetFlag(Message::Flag::Handled, true);
        SetValFromScreenCoord(point.x());
        return new SliderActiveState(m_pData, 0.0f);
      }

      return nullptr;
    }

    Slider::InternalState * SliderStaticState::HandleMessage(Message_GUI_PointerMove * a_pMsg)
    {
      vec2 point((float)a_pMsg->x, (float)a_pMsg->y);

      UIAABB bar, caret;
      GetAABBs(bar, caret);
      bool isInside = PointInBox(vec2((float)a_pMsg->x, (float)a_pMsg->y), caret);
      if (isInside)
        a_pMsg->ConsumeHover();

      if (isInside && m_state == WidgetState::None)
      {
        m_state = WidgetState::HoverOn;
        if (m_pData->m_clbk_HoverOn != nullptr)
          m_pData->m_clbk_HoverOn();
      }
      if (!isInside && m_state == WidgetState::HoverOn)
      {
        m_state = WidgetState::None;
        if (m_pData->m_clbk_HoverOff != nullptr)
          m_pData->m_clbk_HoverOff();
      }
      return nullptr;
    }

    //------------------------------------------------------------------------------------
    // SliderActiveState
    //------------------------------------------------------------------------------------

    SliderActiveState::SliderActiveState(Data * a_pData, float a_offset)
      : InternalState(a_pData)
      , m_offset(a_offset)
    {

    }

    SliderActiveState::~SliderActiveState()
    {

    }

    WidgetState SliderActiveState::QueryState() const
    {
      return WidgetState::HasFocus;
    }

    void SliderActiveState::Draw()
    {
      _Draw(SliderState::Grab);
    }

    Slider::InternalState * SliderActiveState::HandleMessage(Message * a_pMsg)
    {
      if (a_pMsg->GetCategory() != MC_GUI)
        return nullptr;

      if (a_pMsg->GetID() == Message_GUI_PointerUp::GetStaticID())
      {
        a_pMsg->SetFlag(Message::Flag::Handled, true);
        return new SliderStaticState(m_pData, WidgetState::HoverOn);
      }

      if (a_pMsg->GetID() == Message_GUI_PointerMove::GetStaticID())
        return HandleMessage(dynamic_cast<Message_GUI_PointerMove *>(a_pMsg));

      return nullptr;
    }

    Slider::InternalState * SliderActiveState::HandleMessage(Message_GUI_PointerMove * a_pMsg)
    {
      vec2 point((float)a_pMsg->x, (float)a_pMsg->y);
      bool canMove = true;

      if (m_pData->pParent)
      {
        UIAABB aabb;
        if (!m_pData->pParent->GetGlobalAABB(aabb) || !PointInBox(point, aabb))
            canMove = false;
      }
      
      if (canMove)
      {
        SetValFromScreenCoord(point.x() + m_offset);
        m_pData->pSlider->NewValueClbk(m_pData->value);
      }

      a_pMsg->SetFlag(Message::Flag::Handled, true);
      return nullptr;
    }

    //------------------------------------------------------------------------------------
    // Slider
    //------------------------------------------------------------------------------------

    Slider::Slider(Widget * a_pParent, vec2 const & a_position, float a_width, float a_value, std::initializer_list<WidgetFlag> a_flags)
      : Widget({WidgetFlag::NotResponsive,
                WidgetFlag::StretchWidth
        }, a_flags)
      , m_pState(nullptr)
    {
      InternalState::Data * pData = new InternalState::Data();
      pData->caretWidth = GetStyle().sliderCaretWidth;
      if (pData->caretWidth < SLIDER_MIN_CARET_WIDTH)
        pData->caretWidth = SLIDER_MIN_CARET_WIDTH;

      pData->width = a_width;
      if (pData->width < (pData->caretWidth + SLIDER_MIN_RUN_PIXELS))
        pData->width = pData->caretWidth + SLIDER_MIN_RUN_PIXELS;

      pData->value = a_value;
      if (pData->value < 0.0f)
        pData->value = 0.0f;
      else if (pData->value > 1.0f)
        pData->value = 1.0f;

      pData->barHeight = GetStyle().sliderBarHeight;
      pData->caretHeight = GetStyle().sliderCaretHeight;
      pData->outlineWidth = GetStyle().outlineWidth;
      pData->position = a_position;
      pData->pSlider = this;
      pData->pParent = a_pParent;

      pData->clr[(int)SliderState::Normal][(int)SliderElement::Lower] = GetStyle().colours[col_SliderLower];
      pData->clr[(int)SliderState::Normal][(int)SliderElement::Upper] = GetStyle().colours[col_SliderUpper];
      pData->clr[(int)SliderState::Normal][(int)SliderElement::Caret] = GetStyle().colours[col_SliderCaret];
      pData->clr[(int)SliderState::Normal][(int)SliderElement::Outline] = GetStyle().colours[col_SliderOutline];

      pData->clr[(int)SliderState::Hover][(int)SliderElement::Lower] = GetStyle().colours[col_SliderLowerHover];
      pData->clr[(int)SliderState::Hover][(int)SliderElement::Upper] = GetStyle().colours[col_SliderUpperHover];
      pData->clr[(int)SliderState::Hover][(int)SliderElement::Caret] = GetStyle().colours[col_SliderCaretHover];
      pData->clr[(int)SliderState::Hover][(int)SliderElement::Outline] = GetStyle().colours[col_SliderOutlineHover];

      pData->clr[(int)SliderState::Grab][(int)SliderElement::Lower] = GetStyle().colours[col_SliderLowerGrab];
      pData->clr[(int)SliderState::Grab][(int)SliderElement::Upper] = GetStyle().colours[col_SliderUpperGrab];
      pData->clr[(int)SliderState::Grab][(int)SliderElement::Caret] = GetStyle().colours[col_SliderCaretGrab];
      pData->clr[(int)SliderState::Grab][(int)SliderElement::Outline] = GetStyle().colours[col_SliderOutlineGrab];

      m_pState = new SliderStaticState(pData, WidgetState::None);
    }

    Slider::~Slider()
    {
      m_pState->Destroy();
      delete m_pState;
    }

    void Slider::_HandleMessage(Message * a_pMsg)
    {
      UpdateState(m_pState->HandleMessage(a_pMsg));
    }

    void Slider::Draw()
    {
      m_pState->Draw();
    }

    void Slider::BindHoverOn(std::function<void()> a_fn)
    {
      m_pState->BindHoverOn(a_fn);
    }

    void Slider::BindHoverOff(std::function<void()> a_fn)
    {
      m_pState->BindHoverOff(a_fn);
    }

    WidgetState Slider::QueryState() const
    {
      return m_pState->QueryState();
    }

    void Slider::SetVal(float a_val)
    {
      m_pState->SetVal(a_val);
    }

    Widget * Slider::GetParent() const
    {
      return m_pState->GetParent();
    }

    void Slider::SetParent(Widget * a_pParent)
    {
      m_pState->SetParent(a_pParent);
    }

    void Slider::UpdateState(InternalState * a_pState)
    {
      if (a_pState != nullptr)
      {
        delete m_pState;
        m_pState = a_pState;
      }
    }

    void Slider::_SetLocalPosition(vec2 const & a_pos)
    {
      m_pState->_SetLocalPosition(a_pos);
    }

    void Slider::_SetSize(vec2 const & a_size)
    {
      m_pState->_SetSize(a_size);
    }

    vec2 Slider::_GetLocalPosition()
    {
      return m_pState->_GetLocalPosition();
    }

    vec2 Slider::_GetSize()
    {
      return m_pState->_GetSize();
    }

    void Slider::SetColour(SliderState a_state, SliderElement a_ele, Colour a_clr)
    {
      m_pState->SetColour(a_state, a_ele, a_clr);
    }

    //------------------------------------------------------------------------------------
    // SliderFloat
    //------------------------------------------------------------------------------------

    SliderFloat::SliderFloat(Widget * a_pParent, vec2 const & a_position, float a_width,
                             float a_val, float a_minVal, float a_maxVal,
                             std::initializer_list<WidgetFlag> a_flags)
      : Slider(a_pParent, a_position, a_width, 0.0f, a_flags)
      , m_minVal(a_minVal)
      , m_maxVal(a_maxVal)
      , m_lastValue(0.0f)
    {
      if (m_minVal > m_maxVal)
        m_minVal = m_maxVal;

      if (a_val < m_minVal)
        a_val = m_minVal;
      else if (a_val > m_maxVal)
        a_val = m_maxVal;

      SetVal((a_val - m_minVal) / (m_maxVal - m_minVal));
      m_lastValue = a_val;
    }

    SliderFloat * SliderFloat::Create(Widget * a_pParent, vec2 const & a_position, float a_width,
                                      float a_val, float a_minVal, float a_maxVal,
                                      std::initializer_list<WidgetFlag> a_flags)
    {
      return new SliderFloat(a_pParent, a_position, a_width, a_val, a_minVal, a_maxVal, a_flags);
    }

    void SliderFloat::BindNewValue(std::function<void(float)> a_fn)
    {
      m_clbk_NewValue = a_fn;
    }

    void SliderFloat::NewValueClbk(float a_val)
    {
      if (m_clbk_NewValue != nullptr)
      {
        float val = m_minVal + a_val * (m_maxVal - m_minVal);
        if (val != m_lastValue)
        {
          m_clbk_NewValue(m_minVal + a_val * (m_maxVal - m_minVal));
          m_lastValue = val;
        }
      }
    }
  }
}