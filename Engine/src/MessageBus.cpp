//@group Messages

#include "Message.h"
#include "MessageBus.h"
#include "System.h"
#include "SystemStack.h"
#include "options.h"

namespace Engine
{
  MessageBus * MessageBus::s_instance = nullptr;

  void MessageBus::Init(SystemStack& a_ls)
  {
    s_instance = new MessageBus(a_ls);
  }

  void MessageBus::ShutDown()
  {
    delete s_instance;
    s_instance = nullptr;
  }

  MessageBus * MessageBus::Instance()
  {
    return s_instance;
  }

  MessageBus::MessageBus(SystemStack & a_ss)
    : m_systemStack(a_ss)
    , m_producerIndex(0)
  {

  }

  size_t MessageBus::MessageCount()
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_messageQueue[m_producerIndex].size();
  }

  void MessageBus::SwapBuffers()
  {
    m_producerIndex = (m_producerIndex + 1) % 2;
  }

  void * MessageBus::_ReserveAndRegister(size_t a_msgSize)
  {
    m_mutex.lock();
    void * buf = m_buf[m_producerIndex].Allocate(a_msgSize);
    m_messageQueue[m_producerIndex].push_back(static_cast<Message *>(buf));
    m_mutex.unlock();
    return buf;
  }

  void MessageBus::Register(TRef<Message> const & a_message)
  {
    size_t sze = a_message->Size();
    m_mutex.lock();
    void * buf = m_buf[m_producerIndex].Allocate(sze);
    m_messageQueue[m_producerIndex].push_back(static_cast<Message *>(buf));
    m_mutex.unlock();
    a_message->Clone(buf);
  }

  void MessageBus::DispatchMessages()
  {
    int ind = (m_producerIndex + 1) % 2;
    for (size_t i = 0; i < m_messageQueue[ind].size(); i++)
    {
      Message* pMsg = m_messageQueue[ind][i];
      auto it = m_systemStack.begin();
      for (; it != m_systemStack.end(); it++)
      {
        it->second->HandleMessage(pMsg);
        if (pMsg->QueryFlag(Message::Flag::Handled))
          break;
      }
    }
    m_messageQueue[ind].clear();
    m_buf[ind].clear();
  }
}
