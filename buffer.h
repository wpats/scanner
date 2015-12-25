// Circular buffer declaration and associated structures.

#pragma once

#include <list>
#include <vector>
#include <cstdint>

template <typename ElementType>
class ProcessInterface
{
  bool m_doMergeRequests;
 public:
  ProcessInterface(bool doMergeRequests)
    : m_doMergeRequests(doMergeRequests)
  {}
  bool GetDoMergeRequests()
  {
    return this->m_doMergeRequests;
  }
  virtual void Begin(uint64_t sequenceId, uint32_t totalItemCount) = 0;
  virtual void Process(const ElementType * items, uint32_t count) = 0;
  virtual void End() = 0;
};

template <typename ElementType, uint32_t BufferSize> class CircularBuffer
{
  typedef CircularBuffer<ElementType, BufferSize> Self;
  typedef std::pair<uint64_t, uint32_t> RequestType;
  typedef std::list<RequestType> RequestListType;

  struct Buffer
  {
    bool isWritten;
    ElementType data[BufferSize];
  };
  struct Iterator
  {
    Self * m_container;
    uint32_t m_bufferIndex;
    uint32_t m_itemIndex;
  Iterator(Self * container, uint32_t bufferIndex, uint32_t itemIndex)
  : m_container(container),
      m_bufferIndex(bufferIndex), 
      m_itemIndex(itemIndex)
    {}
    Iterator operator++();
    Iterator operator--();
    Iterator operator+(uint32_t count);
    Iterator operator-(uint32_t count);
    bool operator!=(const Iterator & other) const;
    bool operator==(const Iterator & other) const;
  };

  friend class Iterator;
  std::vector<Buffer *> m_buffers;
  RequestListType m_requests;
  uint32_t m_bufferCount;
  uint64_t m_endSequenceId;
  Iterator m_bufferBegin;
  Iterator m_bufferEnd;
  bool m_isEmpty;
  bool m_finished;
  const char * m_outFileName;
  FILE * m_outFile;
      
  uint32_t NextIndex(uint32_t index);
  uint32_t PreviousIndex(uint32_t index);
  uint32_t GetBufferItemBegin(uint32_t bufferIndex);
  uint32_t GetBufferItemEnd(uint32_t bufferIndex);
  uint32_t AppendBlock(const ElementType * items, uint32_t count);
  uint32_t GetFreeItemsAtEnd() const;
  uint32_t GetValidItemsAt(Iterator iter) const;
  ElementType * GetItemPointer(Iterator iter);
  void UpdateBufferEnd(uint32_t itemCount);
  uint32_t AddRequest(uint64_t sequenceId, uint32_t count);
 public:
  CircularBuffer(uint32_t bufferCount, const char * outFileName);
  ~CircularBuffer();
  uint32_t GetItemCount() const;
  uint32_t GetItemCapacity() const;
  bool IsFull() const;
  bool IsEmpty() const;
  bool AppendItems(const ElementType * items, uint32_t count);
  uint32_t ProcessEndItems(uint32_t count, ProcessInterface<ElementType> * process);
  uint32_t ProcessBeginItems(uint32_t count, ProcessInterface<ElementType> * process);
  uint32_t ProcessItems(uint64_t sequenceId, uint32_t count, ProcessInterface<ElementType> * process);
  uint64_t GetEndSequenceId();
};

#ifdef INCLUDE_TEST_MAIN

class FileWriteProcessInterface : public ProcessInterface<uint32_t>
{
  uint32_t m_count;
  uint32_t m_expectedCount;
  uint32_t m_currentValue;
  FILE * m_outFile;
 public:
  FileWriteProcessInterface(const char * outFileName);
  ~FileWriteProcessInterface();
  void Begin(uint64_t sequenceId, uint32_t totalItemCount);
  void Process(const uint32_t * items, uint32_t count);
  void End();
};

#else

class FileWriteProcessInterface : public ProcessInterface<fftwf_complex>
{
  uint32_t m_count;
  uint32_t m_expectedCount;
  uint32_t m_currentValue;
  FILE * m_outFile;
 public:
  FileWriteProcessInterface(const char * outFileName);
  ~FileWriteProcessInterface();
  void Begin(uint64_t sequenceId, uint32_t totalItemCount);
  void Process(const fftwf_complex * items, uint32_t count);
  void End();
};

#endif

class CopyBufferProcessInterface : public ProcessInterface<fftwf_complex>
{
  uint32_t m_count;
  uint32_t m_expectedCount;
  fftwf_complex * m_outputBuffer;
 public:
  CopyBufferProcessInterface(fftwf_complex * outputBuffer);
  ~CopyBufferProcessInterface();
  void Begin(uint64_t sequenceId, uint32_t totalItemCount);
  void Process(const fftwf_complex * items, uint32_t count);
  void End();
};
