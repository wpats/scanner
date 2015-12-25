#include <vector>
#include <stdio.h>
#include <memory.h>
#include <assert.h>
#include <stdlib.h>
#include "fft.h"
#include "buffer.h"

#ifdef INCLUDE_TEST_MAIN
#include <iostream>
#include <random>
#include <functional>
#endif

// template class CircularBuffer<fftwf_complex, 8192*16>;

// Class CircularBuffer methods.
template <typename ElementType, uint32_t BufferSize> 
CircularBuffer<ElementType, BufferSize>::CircularBuffer(uint32_t bufferCount, const char * outFileName)
  : m_buffers(bufferCount, NULL),
    m_bufferCount(bufferCount),
    m_endSequenceId(0),
    m_bufferBegin(this, 0, 0),
    m_bufferEnd(this, 0, 0),
    m_isEmpty(true),
    m_outFileName(outFileName),
    m_outFile(nullptr)
{
  // Having a single buffer means you need to handle corner cases.
  assert(bufferCount > 1);
  for (uint32_t i = 0; i < bufferCount; i++) {
    this->m_buffers[i] = new Buffer;
  }
  if (outFileName != nullptr) {
    this->m_outFile = fopen(outFileName, "w");
    if (this->m_outFile == nullptr) {
      fprintf(stderr, "Error opening file '%s'\n", outFileName);
      exit(1);
    }
  }
}

template <typename ElementType, uint32_t BufferSize> 
CircularBuffer<ElementType, BufferSize>::~CircularBuffer()
{
}

template <typename ElementType, uint32_t BufferSize> 
typename CircularBuffer<ElementType, BufferSize>::Iterator 
CircularBuffer<ElementType, BufferSize>::Iterator::operator++()
{
  return *this + 1;
}

template <typename ElementType, uint32_t BufferSize> 
typename CircularBuffer<ElementType, BufferSize>::Iterator 
CircularBuffer<ElementType, BufferSize>::Iterator::operator--()
{
  return *this - 1;
}

template <typename ElementType, uint32_t BufferSize> 
typename CircularBuffer<ElementType, BufferSize>::Iterator 
CircularBuffer<ElementType, BufferSize>::Iterator::operator+(uint32_t count)
{
  Iterator result(*this);
  uint32_t blockItemCount = BufferSize - result.m_itemIndex;
  if (blockItemCount == count) {
    result.m_bufferIndex = this->m_container->NextIndex(result.m_bufferIndex);
    result.m_itemIndex = 0;
    return result;
  } else if (blockItemCount > count) {
    result.m_itemIndex += count;
    return result;
  }
  result.m_bufferIndex = this->m_container->NextIndex(result.m_bufferIndex);
  count -= blockItemCount;
  uint32_t fullBufferCount = count / BufferSize;
  result.m_bufferIndex += fullBufferCount;
  result.m_bufferIndex %= this->m_container->m_bufferCount;
  count -= fullBufferCount * BufferSize;
  result.m_itemIndex = count;
  return result;
}

template <typename ElementType, uint32_t BufferSize> 
typename CircularBuffer<ElementType, BufferSize>::Iterator 
CircularBuffer<ElementType, BufferSize>::Iterator::operator-(uint32_t count)
{
  Iterator result(*this);
  Iterator check = *this + (this->m_container->GetItemCapacity() - count);
  // Compute the bufferStart and itemIndexStart going backwards
  // count items from the end.
  while (count > 0) {
    uint32_t bufferItemBegin = this->m_container->GetBufferItemBegin(result.m_bufferIndex);
    uint32_t bufferItemEnd = this->m_container->GetBufferItemEnd(result.m_bufferIndex);
    uint32_t itemCount = bufferItemEnd - bufferItemBegin;
    if (count <= itemCount) {
      bufferItemBegin = bufferItemEnd - count;
      result.m_itemIndex = bufferItemBegin;
      count = 0;
    } else {
      count -= itemCount;
      result.m_bufferIndex = this->m_container->PreviousIndex(result.m_bufferIndex);
    }
  }
  // assert(check == result);
  return check;
}

template <typename ElementType, uint32_t BufferSize> 
bool
CircularBuffer<ElementType, BufferSize>::Iterator::operator!=(const Iterator & other)const
{
  return memcmp(this, &other, sizeof(*this));
}

template <typename ElementType, uint32_t BufferSize> 
bool
CircularBuffer<ElementType, BufferSize>::Iterator::operator==(const Iterator & other)const
{
  return !memcmp(this, &other, sizeof(*this));
}

template <typename ElementType, uint32_t BufferSize> 
uint32_t CircularBuffer<ElementType, BufferSize>::GetItemCount() const
{
  uint32_t count = 0;
  if (this->IsFull()) {
    return this->GetItemCapacity();
  }
  Iterator iter(this->m_bufferBegin);
  while (iter != this->m_bufferEnd) {
    uint32_t n = this->GetValidItemsAt(iter);
    count += n;
    iter = iter + n;
  }
  return count;
}
    
template <typename ElementType, uint32_t BufferSize> 
uint32_t CircularBuffer<ElementType, BufferSize>::GetItemCapacity() const
{
  return this->m_bufferCount * BufferSize;
}
    
template <typename ElementType, uint32_t BufferSize> 
uint32_t CircularBuffer<ElementType, BufferSize>::NextIndex(uint32_t index)
{
  return (index + 1)%this->m_bufferCount;
}

template <typename ElementType, uint32_t BufferSize> 
bool CircularBuffer<ElementType, BufferSize>::IsFull() const
{
  return !this->m_isEmpty && this->m_bufferBegin == this->m_bufferEnd;
}

template <typename ElementType, uint32_t BufferSize> 
bool CircularBuffer<ElementType, BufferSize>::IsEmpty() const
{
  return this->m_isEmpty;
}


template <typename ElementType, uint32_t BufferSize> 
uint32_t CircularBuffer<ElementType, BufferSize>::PreviousIndex(uint32_t index)
{
  if (index == 0) {
    return this->m_bufferCount - 1;
  } else {
    return index - 1;
  }
}

template <typename ElementType, uint32_t BufferSize> 
uint32_t CircularBuffer<ElementType, BufferSize>::GetBufferItemBegin(uint32_t bufferIndex)
{
  if (this->m_bufferBegin.m_bufferIndex != this->m_bufferEnd.m_bufferIndex) {
    if (this->m_bufferBegin.m_bufferIndex == bufferIndex) {
      return this->m_bufferBegin.m_itemIndex;
    }
    return 0;
  } else if (this->m_bufferBegin.m_bufferIndex != bufferIndex) {
    return 0;
  } else if (this->m_bufferEnd.m_itemIndex <= this->m_bufferBegin.m_itemIndex) {
    assert(this->m_bufferBegin.m_bufferIndex == bufferIndex);
    return 0;
  } else {
    return this->m_bufferBegin.m_itemIndex;
  }
}

template <typename ElementType, uint32_t BufferSize> 
uint32_t CircularBuffer<ElementType, BufferSize>::GetBufferItemEnd(uint32_t bufferIndex)
{
  if (this->m_bufferBegin.m_bufferIndex != this->m_bufferEnd.m_bufferIndex) {
    if (this->m_bufferEnd.m_bufferIndex == bufferIndex) {
      return this->m_bufferEnd.m_itemIndex;
    }
    return BufferSize;
  } else if (this->m_bufferBegin.m_bufferIndex != bufferIndex) {
    return BufferSize;
  } else {
    assert(this->m_bufferBegin.m_bufferIndex == bufferIndex);
    return this->m_bufferEnd.m_itemIndex;
  }
}

template <typename ElementType, uint32_t BufferSize> 
uint32_t CircularBuffer<ElementType, BufferSize>::AppendBlock(const ElementType * items, uint32_t count)
{
  uint32_t blockItemCount;
  Iterator iter = this->m_bufferEnd;
  ElementType * destination = this->GetItemPointer(iter);
  blockItemCount = this->GetFreeItemsAtEnd();
  blockItemCount = std::min(count, blockItemCount);
  uint32_t copySize = blockItemCount * sizeof(ElementType);
  memcpy(destination, items, copySize);
  this->UpdateBufferEnd(blockItemCount);
  return blockItemCount;
}

template <typename ElementType, uint32_t BufferSize> 
void CircularBuffer<ElementType, BufferSize>::UpdateBufferEnd(uint32_t itemCount)
{
  assert(itemCount > 0);
  // Check for overflow.
  bool overflow = this->GetItemCount() + itemCount > this->GetItemCapacity();
  this->m_isEmpty = false;
  this->m_bufferEnd = this->m_bufferEnd + itemCount;
  this->m_endSequenceId += itemCount;
  if (overflow) {
    this->m_bufferBegin = this->m_bufferEnd;
    return;
  }
}

// Return a count of how many free items are in the block.
//
template <typename ElementType, uint32_t BufferSize> 
uint32_t CircularBuffer<ElementType, BufferSize>::GetFreeItemsAtEnd() const
{
  return BufferSize - this->m_bufferEnd.m_itemIndex;
}

// Return a count of how many input items are available in the block.
//
template <typename ElementType, uint32_t BufferSize> 
uint32_t CircularBuffer<ElementType, BufferSize>::GetValidItemsAt(Iterator iter) const
{
  uint32_t itemIndexEnd;
  if (this->m_bufferEnd.m_bufferIndex == iter.m_bufferIndex
      && iter.m_itemIndex <= this->m_bufferEnd.m_itemIndex) {
    if (this->m_bufferEnd == this->m_bufferBegin) {
      itemIndexEnd = BufferSize;
    } else {
      itemIndexEnd = this->m_bufferEnd.m_itemIndex;
    }
  } else {
    itemIndexEnd = BufferSize;
  }
  assert(itemIndexEnd >= iter.m_itemIndex);
  return itemIndexEnd - iter.m_itemIndex;
}

// Return a count of how many free items are in the block.
//
template <typename ElementType, uint32_t BufferSize> 
ElementType * CircularBuffer<ElementType, BufferSize>::GetItemPointer(Iterator iter)
{
  ElementType * elements = 
    &this->m_buffers[iter.m_bufferIndex]->data[iter.m_itemIndex];
  return elements;
}

// Add the specified request to the list and determine if there is overlap with
// the previous request. If so return the new item count.
template <typename ElementType, uint32_t BufferSize> 
uint32_t CircularBuffer<ElementType, BufferSize>::AddRequest(uint64_t sequenceId, uint32_t count)
{
  RequestListType::iterator iter = this->m_requests.begin();
  RequestListType::iterator end = this->m_requests.end();
  if (iter != end) {
    // assert(iter->first <= sequenceId);
    uint64_t requestEndId = iter->first + iter->second;
    // Test for overlap.
    if (requestEndId >= sequenceId) {
      if (requestEndId < sequenceId + count) {
        uint32_t difference = sequenceId + count - requestEndId;
#if 0
        if (iter->second + difference <= this->GetItemCapacity()) {
          iter->second += difference;
          printf("Merging request [%lu, %u]\n", iter->first, iter->second);
          return difference;
        }
#endif
        count = difference;
        sequenceId = requestEndId;
      } else {
        // The previous request completely covers the old, so nothing to
        // add.
        return 0;
      }
    }
  }
  this->m_requests.push_front(std::make_pair(sequenceId, count));
  // printf("Adding request [%lu, %u]\n", sequenceId, count);
  return count;
}

template <typename ElementType, uint32_t BufferSize> 
bool CircularBuffer<ElementType, BufferSize>::AppendItems(const ElementType * items, uint32_t count)
{
  const ElementType * cursor = items;
  while (count > 0) {
    uint32_t saveCount = this->AppendBlock(cursor, count);
    cursor += saveCount;
    count -= saveCount;
  }

}

template <typename ElementType, uint32_t BufferSize> 
uint64_t CircularBuffer<ElementType, BufferSize>::GetEndSequenceId()
{
  return this->m_endSequenceId;
}
    
template <typename ElementType, uint32_t BufferSize> 
uint32_t CircularBuffer<ElementType, BufferSize>::ProcessEndItems(uint32_t count, 
                                                                  ProcessInterface<ElementType> * process)
{
  count = std::min(count, this->GetItemCount());
  return this->ProcessItems(this->m_endSequenceId - count, count, process);
}

template <typename ElementType, uint32_t BufferSize> 
uint32_t CircularBuffer<ElementType, BufferSize>::ProcessBeginItems(uint32_t count, 
                                                                    ProcessInterface<ElementType> * process)
{
  count = std::min(count, this->GetItemCount());
  return this->ProcessItems(this->m_endSequenceId - this->GetItemCount(), count, process);
}

template <typename ElementType, uint32_t BufferSize> 
uint32_t CircularBuffer<ElementType, BufferSize>::ProcessItems(uint64_t sequenceId, 
                                                               uint32_t count, 
                                                               ProcessInterface<ElementType> * process)
{
  assert(sequenceId <= this->m_endSequenceId);
  count = std::min(count, this->GetItemCount());
  if (process->GetDoMergeRequests()) {
    count = this->AddRequest(sequenceId, count);
  }
  uint32_t returnCount = count;
  if (count == 0) {
    return 0;
  }
  process->Begin(sequenceId, count);
  uint32_t countFromEnd = this->m_endSequenceId - sequenceId;
  assert(countFromEnd <= this->GetItemCount());
  Iterator iter = this->m_bufferEnd - countFromEnd;
  while (count > 0) {
    uint32_t bufferItemCount;
    ElementType * source = this->GetItemPointer(iter);
    bufferItemCount = std::min(count, this->GetValidItemsAt(iter));
    process->Process(source, bufferItemCount);
    count -= bufferItemCount;
    iter = iter + bufferItemCount;
  }
  process->End();
  return returnCount;
}

#ifdef INCLUDE_TEST_MAIN

#include <vector>
#include <stdio.h>
#include <memory.h>
#include <assert.h>
#include <stdlib.h>

FileWriteProcessInterface::FileWriteProcessInterface(const char * outFileName)
  : m_count(0),
    m_expectedCount(0)
{
  if (outFileName != NULL) {
    this->m_outFile = fopen(outFileName, "w");
    if (this->m_outFile == NULL) {
      fprintf(stderr, "Error opening file '%s'\n", outFileName);
      exit(1);
    }
  }
}

FileWriteProcessInterface::~FileWriteProcessInterface()
{
  if (this->m_outFile != NULL) {
    fclose(this->m_outFile);
  }
}

void FileWriteProcessInterface::Begin(uint64_t sequenceId, uint32_t totalItemCount)
{
  if (this->m_outFile != NULL) {
    this->m_expectedCount = totalItemCount;
    this->m_currentValue = uint32_t(sequenceId);

    printf("Begin writing [%lu, %u]...", sequenceId, totalItemCount);
  }
}

void FileWriteProcessInterface::Process(const uint32_t * items, uint32_t count)
{
  if (this->m_outFile != NULL) {
    printf("Processing [%u, %u]...", this->m_currentValue, count);
    this->m_count += count;
    for (uint32_t i = 0; i < count; i++) {
      fprintf(this->m_outFile, "%u\n", items[i]);
      assert(items[i] == this->m_currentValue);
      this->m_currentValue++;
    }
  }
}
    
void FileWriteProcessInterface::End()
{
  if (this->m_expectedCount == this->m_count) {
    printf("OK\n");
  } else {
    printf("Got %u items, expected %u\n", this->m_count, this->m_expectedCount);
  }
  this->m_count = this->m_expectedCount = 0;
}

int main()
{
  printf("Entering main...\n");
  // CircularBuffer<float, 100000> buffer(100);
  CircularBuffer<uint32_t, 100000> buffer(100, nullptr);
  const int nInputs = 1000000;
  // float input[nInputs];
  uint32_t input[nInputs];
  uint32_t counter = 0;
  FileWriteProcessInterface writeInterface(nullptr);

  std::default_random_engine generator;
#if 0
  std::uniform_real_distribution<float> fdistribution(-1.0, 1.0);
  for (int i = 0; i < nInputs; i++) {
    input[i] = fdistribution(generator);
  }
#endif
  printf("Generating inputs\n");
  std::uniform_int_distribution<int> idistribution(2000, 10000);
  uint64_t totalSize = 0;
  int iterations = 100000;
  while (--iterations) {
    uint32_t count = idistribution(generator);
    totalSize += count;
    for (int i = 0; i < count; i++) {
      input[i] = counter++;
    }
    buffer.AppendItems(&input[0], count);
    printf("Appending %d items, totalSize = %ul[%ul], buffer count = %u\n", 
           count, 
           totalSize,
           totalSize%buffer.GetItemCapacity(),
           buffer.GetItemCount());
    if (idistribution(generator) > 9500) { // totalSize > 5000000) {
      buffer.ProcessItems(4000000, &writeInterface);
      // totalSize -= 4000000;
    }
  }

  return 0;
}

#endif
