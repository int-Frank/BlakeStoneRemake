#include "DoubleBufferWithAllocs.h"

//------------------------------------------------------------------------------------------
// DoubleBufferWithAllocs::Ref
//------------------------------------------------------------------------------------------

DoubleBufferWithAllocs::Ref::Ref()
  : m_nProdRefs(nullptr)
  , m_buf(nullptr)
{

}

DoubleBufferWithAllocs::Ref::Ref(std::atomic<size_t>* a_nProdRefs, void* a_buf)
  : m_nProdRefs(a_nProdRefs)
  , m_buf(a_buf)
{
  BSR_ASSERT(a_nProdRefs && a_buf, "");
  (*m_nProdRefs)++;
}

DoubleBufferWithAllocs::Ref::~Ref()
{
  if (m_nProdRefs)
    (*m_nProdRefs)--;
}

DoubleBufferWithAllocs::Ref::Ref(Ref const& a_other)
  : m_nProdRefs(a_other.m_nProdRefs)
  , m_buf(a_other.m_buf)
{
  if (m_nProdRefs)
    (*m_nProdRefs)++;
}

typename DoubleBufferWithAllocs::Ref&
DoubleBufferWithAllocs::Ref::operator=(Ref const& a_other)
{
  if (this != &a_other)
  {
    m_buf = a_other.m_buf;
    if (m_nProdRefs)
      (*m_nProdRefs)--;
    m_nProdRefs = a_other.m_nProdRefs;
    if (m_nProdRefs)
      (*m_nProdRefs)++;
  }

  return *this;
}

DoubleBufferWithAllocs::Ref::Ref(Ref&& a_other)
  : m_nProdRefs(a_other.m_nProdRefs)
  , m_buf(a_other.m_buf)
{
  a_other.m_buf = nullptr;
  a_other.m_nProdRefs = nullptr;
}

typename DoubleBufferWithAllocs::Ref&
DoubleBufferWithAllocs::Ref::operator=(Ref&& a_other)
{
  if (this != &a_other)
  {
    if (m_nProdRefs)
      (*m_nProdRefs)--;

    m_buf = a_other.m_buf;
    m_nProdRefs = a_other.m_nProdRefs;
    a_other.m_buf = nullptr;
    a_other.m_nProdRefs = nullptr;
  }

  return *this;
}

//------------------------------------------------------------------------------------------
// DoubleBufferWithAllocs
//------------------------------------------------------------------------------------------

void* DoubleBufferWithAllocs::Ref::GetBuffer() const
{
  return m_buf;
}

DoubleBufferWithAllocs::DoubleBufferWithAllocs(size_t a_size)
  : m_producerIndex(0)
  , m_nProdRefs{0, 0}
  , m_buffer{MemBuffer(a_size), MemBuffer(a_size)}
{

}

DoubleBufferWithAllocs::DoubleBufferWithAllocs(size_t a_size, size_t a_alignment)
  : m_producerIndex(0)
  , m_nProdRefs{0, 0}
  , m_buffer{MemBuffer(a_size, a_alignment), MemBuffer(a_size, a_alignment)}
{

}

DoubleBufferWithAllocs::DoubleBufferWithAllocs()
  : m_producerIndex(0)
  , m_nProdRefs{0, 0}
{

}

DoubleBufferWithAllocs::~DoubleBufferWithAllocs()
{
  //Producer threads should have been terminated before buffer is destroyed.
  BSR_ASSERT(m_nProdRefs[0] == 0 && m_nProdRefs[1] == 0, "");
}

typename DoubleBufferWithAllocs::Ref
DoubleBufferWithAllocs::Allocate(size_t a_size)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  void* ptr = m_buffer[m_producerIndex].Allocate(a_size);
  m_allocations[m_producerIndex].push_back(ptr);
  return Ref(&m_nProdRefs[m_producerIndex], ptr);
}

void DoubleBufferWithAllocs::Swap()
{
  //Save the to-be consumer index
  int ind = m_producerIndex;

  //Set new threads to allocate on the new buffer
  m_mutex.lock();
  m_producerIndex = (m_producerIndex + 1) % 2;
  m_nProdRefs[m_producerIndex] = 0;
  m_allocations[m_producerIndex].clear();
  m_mutex.unlock();

  //Wait until no thread is accessing the now consumer buffer.
  if (m_nProdRefs[ind] != 0)
  {
    std::mutex mut;
    std::unique_lock<std::mutex> lock(mut);
    m_cv.wait(lock, [this, ind = ind]
      {
        return m_nProdRefs[ind] == 0;
      });
  }
}

MemBuffer& DoubleBufferWithAllocs::GetBuffer()
{
  size_t ind = (m_producerIndex + 1) % 2;
  return m_buffer[ind];
}

PODArray<void*>& DoubleBufferWithAllocs::GetAllocations()
{
  size_t ind = (m_producerIndex + 1) % 2;
  return m_allocations[ind];
}

size_t DoubleBufferWithAllocs::GetCurrentRefCount()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_nProdRefs[m_producerIndex];
}

