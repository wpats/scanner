#pragma once

#include <vector>
#include <atomic>
#include <thread>
#include <boost/circular_buffer.hpp>
#include "fft.h"
#include "utility.h"
#include "memoryPool.h"

template <typename T> 
class MessageQueue
{
 public:
  struct MessageHeader
  {
    enum MessageKind {
      Illegal = 0,
      ProcessData,
      WriteData,
      WriteDataAndStop,
      Free
    } m_kind;
    uint32_t m_referenceCount;
    double m_frequency;
    uint64_t m_sequenceId;
    time_t m_time;
  };
  typedef MemoryPool<MessageHeader, T> Allocator;
  typedef typename Allocator::BufferType MessageType;
  enum SampleKind {
    Illegal = 0,
    ByteComplex,
    Short,
    ShortComplex,
    FloatComplex
  } m_kind;
 private:
  boost::circular_buffer<MessageType *> m_buffer;
  boost::circular_buffer<MessageType *> m_writeBuffer;
  Allocator m_memoryPool;
  std::mutex m_mutex;
  std::condition_variable m_conditionEmpty;
  std::condition_variable m_conditionFull;
  // Members related to write thread which writes samples to file.
  std::mutex m_writeMutex;
  std::condition_variable m_conditionDoWrite;
  std::condition_variable m_conditionWriteEmpty;
  std::unique_ptr<std::thread> m_writeThread;
  FILE * m_writeFile;
  uint64_t m_writeStartSequenceId;
  uint64_t m_writeEndSequenceId;
  bool m_doWrite;
  uint32_t m_iterationCount;

  // Not related to writing.
  fftwf_complex * m_floatComplex;
  uint64_t m_nextBufferSequenceId;
  uint32_t m_enob;
  uint32_t m_sampleCount;
  bool m_correctDCOffset;
  bool m_done;
  uint32_t enob;
  void SynchronizedAppend(T * data, double centerFrequency, time_t time)
  {
    if (time) {
      this->m_iterationCount++;
    }
    if (this->m_iterationCount < 2) {
      return;
    }
    MessageType * message = this->m_memoryPool.Allocate();
    memset(message->GetData(), 0, sizeof(T) * this->m_sampleCount);
    memcpy(message->GetData(), data, sizeof(T) * this->m_sampleCount);
    MessageHeader & header = message->GetHeader();
    header.m_time = time;
    header.m_frequency = centerFrequency;
    header.m_kind = MessageHeader::ProcessData;
    std::unique_lock<std::mutex> locker(this->m_mutex);
    header.m_sequenceId = this->m_nextBufferSequenceId++;
    while (this->IsFull()) {
      this->m_conditionFull.wait(locker);
    }
    bool wake = this->IsEmpty();
    this->m_buffer.push_front(message);
    if (wake) {
      this->m_conditionEmpty.notify_one();
    }
  }
  bool IsFull() {
    return this->m_buffer.full();
  }
  bool IsEmpty() {
    return this->m_buffer.empty();
  }
  void WriteThreadWorker() {
    while (true) {
      if (this->GetIsDone()) {
        break;
      }
      {
        std::unique_lock<std::mutex> locker(this->m_writeMutex);
        this->m_conditionDoWrite.wait(locker);
        if (this->GetIsDone()) {
          break;
        }
        uint64_t startSequenceId = this->m_writeStartSequenceId;
        auto iter = std::find_if(this->m_writeBuffer.rbegin(), 
                                 this->m_writeBuffer.rend(),
                                 [=](MessageType * & item) {
                                   return (item->GetHeader().m_sequenceId == startSequenceId);
                                 });
        bool done = false;
        while (!done) {
          while (!this->GetIsDone() && iter == this->m_writeBuffer.rend()) {
            this->m_conditionWriteEmpty.wait(locker);
          }
          if (this->GetIsDone()) {
            break;
          }
          MessageType * message = *iter++;
          if (message->GetHeader().m_sequenceId < this->m_writeEndSequenceId) {
            printf("Writing %lu\n", message->GetHeader().m_sequenceId);
            //memset(message->GetData(), 0, sizeof(fftwf_complex) * this->m_sampleCount);
            fwrite(message->GetData(), 
                   sizeof(fftwf_complex), 
                   this->m_sampleCount, 
                   this->m_writeFile);
          } else {
            assert(this->m_writeFile != nullptr);
            fclose(this->m_writeFile);
            done = true;
          }
        }
      }
    }
  }
 public:
  MessageQueue(SampleKind kind, 
               uint32_t enob, 
               uint32_t sampleCount, 
               uint32_t bufferCount, 
               bool correctDCOffset,
               bool doWrite)
    : m_sampleCount(sampleCount),
      m_buffer(bufferCount),
      m_writeBuffer(bufferCount/10),
      m_memoryPool(sampleCount, uint32_t(bufferCount * 1.1)),
      m_enob(enob),
      m_kind(kind),
      m_done(false),
      m_nextBufferSequenceId(0),
      m_correctDCOffset(correctDCOffset),
      m_writeStartSequenceId(0),
      m_writeEndSequenceId(0),
      m_doWrite(doWrite),
      m_iterationCount(0),
      m_writeFile(nullptr)
  {
    assert(kind > Illegal && kind <= FloatComplex);
    this->m_floatComplex = new fftwf_complex[sampleCount];
    if (doWrite) {
      printf("Starting write thread...\n");
      this->m_writeThread = std::unique_ptr<std::thread>(new std::thread(&MessageQueue::WriteThreadWorker, 
                                                                         this));
    }
  }

  ~MessageQueue() {
    assert(this->IsEmpty());
    if (this->m_doWrite) {
      assert(this->m_writeThread != nullptr);
      printf("Stopping write thread...\n");
      // Shut down the thread
      this->m_writeThread->join();
    }
    while (!this->m_writeBuffer.empty()) {
      MessageType * message = this->m_writeBuffer.back();
      this->m_writeBuffer.pop_back();
      this->m_memoryPool.Free(message);
    }
    if (this->m_writeFile != nullptr) {
      fclose(this->m_writeFile);
    }
  }

  void AppendSamples(int16_t * realSamples, 
                     int16_t * imagSamples, 
                     double centerFrequency,
                     time_t time)
  {
    assert(this->m_kind == Short);
    Utility::short_complex_to_float_complex(realSamples, 
                                            imagSamples, 
                                            this->m_floatComplex,
                                            this->m_sampleCount,
                                            this->m_enob,
                                            this->m_correctDCOffset);
    this->SynchronizedAppend(this->m_floatComplex, centerFrequency, time);
  }

  void AppendSamples(int16_t shortComplexSamples[][2],
                     double centerFrequency,
                     time_t time)
  {
    assert(this->m_kind == ShortComplex);
    Utility::short_complex_to_float_complex(shortComplexSamples,
                                            this->m_floatComplex,
                                            this->m_sampleCount,
                                            this->m_enob,
                                            this->m_correctDCOffset);
    this->SynchronizedAppend(this->m_floatComplex, centerFrequency, time);
  }

  void AppendSamples(int8_t byteComplexSamples[][2],
                     double centerFrequency,
                     time_t time)
  {
    assert(this->m_kind == ByteComplex);
    Utility::byte_complex_to_float_complex(byteComplexSamples,
                                           this->m_floatComplex,
                                           this->m_sampleCount,
                                           this->m_enob,
                                           this->m_correctDCOffset);
    this->SynchronizedAppend(this->m_floatComplex, centerFrequency, time);
  }

  void AppendSamples(fftwf_complex * floatComplexSamples,
                     double centerFrequency,
                     time_t time)
  {
    assert(this->m_kind == FloatComplex);   
    this->SynchronizedAppend(floatComplexSamples, centerFrequency, time);
  }

  MessageType * GetNextSamples()
  {
    std::unique_lock<std::mutex> locker(this->m_mutex);
    while (!this->GetIsDone() && this->IsEmpty()) {
      this->m_conditionEmpty.wait(locker);
    }
    if (this->GetIsDone()) {
      if (this->IsEmpty()) {
        return nullptr;
      }
    }
    bool wake = this->IsFull();
    MessageType * message = this->m_buffer.back();
    this->m_buffer.pop_back();
    if (wake) {
      this->m_conditionFull.notify_one();
    }
    return message;
  }

  void MessageProcessed(MessageType * message) {
    assert(message->GetHeader().m_kind != MessageHeader::Illegal);
    // assert(message->GetHeader().m_kind != MessageHeader::ProcessData);
    // MessageType * back = this->m_buffer.back();
    // assert(message->GetHeader().m_sequenceId == back->GetHeader().m_sequenceId);
    // std::unique_lock<std::mutex> locker(this->m_mutex);
    std::unique_lock<std::mutex> locker(this->m_writeMutex);
    if (this->m_writeBuffer.full()) {
      MessageType * message = this->m_writeBuffer.back();
      this->m_writeBuffer.pop_back();
      this->m_memoryPool.Free(message);
    }
    this->m_writeBuffer.push_front(message);
    this->m_conditionWriteEmpty.notify_one();
  }
  
  void BeginWrite(uint64_t startSequenceId, std::string fileName) {
    printf("BeginWrite %s: %lu\n", fileName.c_str(), startSequenceId);
    std::unique_lock<std::mutex> locker(this->m_writeMutex);
    this->m_writeFile = fopen(fileName.c_str(), "w");
    this->m_writeStartSequenceId = startSequenceId;
    this->m_writeEndSequenceId = std::numeric_limits<uint64_t>::max();
    this->m_conditionDoWrite.notify_one();
  }

  void EndWrite(uint64_t sequenceId) {
    printf("EndWrite %lu\n", sequenceId);
    std::unique_lock<std::mutex> locker(this->m_writeMutex);
    this->m_writeEndSequenceId = sequenceId;
  }

  void SetIsDone() {
    assert(!this->m_done);
    // Notify any thread waiting for samples.
    {
      std::unique_lock<std::mutex> locker(this->m_mutex);
      this->m_done = true;
      this->m_conditionEmpty.notify_all();
    }
    // Notify the write thread.
    if (this->m_doWrite) {
      std::unique_lock<std::mutex> locker(this->m_writeMutex);
      this->m_conditionDoWrite.notify_one();
      this->m_conditionWriteEmpty.notify_one();
    }
  }

  // This has an unsynchronized access to the member.
  //
  bool GetIsDone() {
    return this->m_done;
  }
};
  
template class MessageQueue<fftwf_complex>;
typedef class MessageQueue<fftwf_complex> SampleQueue;
