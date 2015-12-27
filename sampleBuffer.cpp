
#include <cassert>
#include <algorithm>
#include "sampleBuffer.h"
#include "process.h"

SampleBuffer::SampleBuffer(SampleKind kind, uint32_t enob, uint32_t sampleCount)
  : m_kind(kind),
    m_enob(enob),
    m_notified(false),
    m_nextOutSequenceId(0),
    m_correctDCOffset(false),
    m_done(false),
    m_circularBuffer(16, nullptr),
    m_sampleCount(sampleCount)
{
  assert(kind > Illegal && kind <= FloatComplex);
  this->m_floatComplex = new fftwf_complex[sampleCount];
}

// This needs to be called before appending to the circular buffer.
//
void SampleBuffer::AppendHelper(double centerFrequency)
{
  uint64_t sequenceId = this->m_circularBuffer.GetEndSequenceId();
  this->m_queue.push_back(std::make_pair(sequenceId, centerFrequency));
}

void SampleBuffer::WriteSamplesToFile(std::string fileName, uint32_t count)
{
  fprintf(stderr, "Writing to file %s\n", fileName.c_str());
  FileWriteProcessInterface writeInterface(fileName.c_str());
  this->m_circularBuffer.ProcessItems(this->m_nextOutSequenceId, count, &writeInterface);
}

void SampleBuffer::SetIsDone()
{
  assert(!this->m_done);
  std::unique_lock<std::mutex> locker(this->m_mutex);
  this->m_done = true;
  this->m_conditionEmpty.notify_one();
}

// This has an unsynchronized access to the member.
//
bool SampleBuffer::GetIsDone()
{
  return this->m_done;
}

// This has an unsynchronized access to the members.
//
bool SampleBuffer::IsFull()
{
  return (this->m_circularBuffer.GetEndSequenceId() - this->m_nextOutSequenceId)
    == this->m_circularBuffer.GetItemCapacity();
}

// This has an unsynchronized access to the members.
//
bool SampleBuffer::IsEmpty()
{
  return this->m_circularBuffer.GetEndSequenceId() == this->m_nextOutSequenceId;
}

double SampleBuffer::GetCenterFrequency(uint64_t sequenceId)
{
  auto iter = std::find_if(this->m_queue.begin(), 
                           this->m_queue.end(), 
                           [=](std::pair<uint64_t, double> & item) -> bool {
                             return (item.first == sequenceId);
                           });
  assert(iter != this->m_queue.end());
  return iter->second;
}

void SampleBuffer::SynchronizedAppend()
{
   std::unique_lock<std::mutex> locker(this->m_mutex);
   while (this->IsFull()) {
     this->m_conditionFull.wait(locker);
  }
   bool wake = this->IsEmpty();
   this->m_circularBuffer.AppendItems(this->m_floatComplex, this->m_sampleCount);
   if (wake) {
     this->m_conditionEmpty.notify_one();
   }
}

void SampleBuffer::AppendSamples(int16_t * realSamples, 
                                 int16_t * imagSamples,
                                 double centerFrequency)
{
   assert(this->m_kind == Short);
   this->AppendHelper(centerFrequency);
   Utility::short_complex_to_float_complex(realSamples, 
                                           imagSamples, 
                                           this->m_floatComplex,
                                           this->m_sampleCount,
                                           this->m_enob,
                                           this->m_correctDCOffset);
   this->SynchronizedAppend();
}

void SampleBuffer::AppendSamples(int16_t shortComplexSamples[][2],
                                 double centerFrequency)
{
   assert(this->m_kind == ShortComplex);
   this->AppendHelper(centerFrequency);
   Utility::short_complex_to_float_complex(shortComplexSamples,
                                           this->m_floatComplex,
                                           this->m_sampleCount,
                                           this->m_enob,
                                           this->m_correctDCOffset);
   this->SynchronizedAppend();
}

void SampleBuffer::AppendSamples(fftwf_complex * floatComplexSamples,
                                 double centerFrequency)
{
   assert(this->m_kind == FloatComplex);   
   this->AppendHelper(centerFrequency);
   this->SynchronizedAppend();
}

bool SampleBuffer::GetNextSamples(fftwf_complex * outputBuffer, 
                                  double & centerFrequency)
{
  std::unique_lock<std::mutex> locker(this->m_mutex);
  while (!this->GetIsDone() 
         && this->IsEmpty()) {
    this->m_conditionEmpty.wait(locker);
  }
  if (this->GetIsDone()) {
    if (this->IsEmpty()) {
      return false;
    }
  }
  bool wake = this->IsFull();
  CopyBufferProcessInterface copyInterface(outputBuffer);
  uint32_t count = this->m_circularBuffer.ProcessItems(this->m_nextOutSequenceId,
                                                       this->m_sampleCount,
                                                       &copyInterface);
  assert(count == this->m_sampleCount);
  centerFrequency = this->GetCenterFrequency(this->m_nextOutSequenceId);
  this->m_nextOutSequenceId += count;
  if (wake) {
    this->m_conditionFull.notify_one();
  }
  return true;
}

#include "buffer.cpp"
