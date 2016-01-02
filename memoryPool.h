#pragma once

#include <list>
#include <mutex>
#include <condition_variable>

template <class HeaderT, typename T>
class Buffer
{
  HeaderT m_header;
  T * m_data;
 public:
  uint32_t m_bufferSize;
  Buffer(uint32_t bufferSize)
    : m_header(),
      m_bufferSize(bufferSize)
  {
    this->m_data = new T[bufferSize];
  }
  ~Buffer() { 
    delete [] this->m_data; 
  }
  HeaderT & GetHeader() {
    return this->m_header;
  }
  T * GetData() {
    return m_data;
  }
};

template <class HeaderT, typename DataT>
class MemoryPool 
{
 public:
  typedef Buffer<HeaderT, DataT> BufferType;
 private:
  uint32_t m_bufferCount;
  std::list<BufferType *> m_free;
  std::mutex m_mutex;
  std::condition_variable m_conditionEmpty;
 public:
  MemoryPool(uint32_t bufferSize, uint32_t bufferCount)
    : m_bufferCount(bufferCount),
      m_free(),
      m_mutex()
      {
        for (uint32_t i = 0; i < bufferCount; i++) {
          m_free.push_back(new BufferType(bufferSize));
        }
      }
  ~MemoryPool() {
    assert(this->m_free.size() == this->m_bufferCount);
    while (!this->m_free.empty()) {
      BufferType * element = this->m_free.front();
      delete element;
      this->m_free.pop_front();
    }
  }
  BufferType * Allocate() {
    std::unique_lock<std::mutex> locker(this->m_mutex);
    while (this->m_free.empty()) {
      this->m_conditionEmpty.wait(locker);
    }
    BufferType * element = this->m_free.front();
    this->m_free.pop_front();
    return element;
  }
  void Free(BufferType * element) {
    std::unique_lock<std::mutex> locker(this->m_mutex);
    bool wasEmpty = this->m_free.empty();
    this->m_free.push_back(element);
    if (wasEmpty) {
      this->m_conditionEmpty.notify_one();
    }
  }
};
