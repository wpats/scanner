#pragma once

#include "fft.h"
#include "buffer.h"
#include <mutex>
#include <condition_variable>

class SampleBuffer
{
  uint32_t m_sampleCount;
  CircularBuffer<fftwf_complex, 8192*16> m_circularBuffer;
  std::list<std::pair<uint64_t, double>> m_queue;
  std::mutex m_mutex;
  std::condition_variable m_conditionEmpty;
  std::condition_variable m_conditionFull;
  bool m_notified;
  fftwf_complex * m_floatComplex;
  uint64_t m_nextOutSequenceId;
  uint32_t m_enob;
  bool m_correctDCOffset;
  bool m_done;
  void AppendHelper(double centerFrequency);
  void SynchronizedAppend();
  double GetCenterFrequency(uint64_t sequenceId);
  bool IsFull();
  bool IsEmpty();
public:
  enum SampleKind {
    Illegal = 0,
    Short,
    ShortComplex,
    FloatComplex
  } m_kind;
  SampleBuffer(SampleKind kind, uint32_t enob, uint32_t count);
  void AppendSamples(int16_t * realSamples, 
                     int16_t * imagSamples, 
                     double centerFrequency);
  void AppendSamples(int16_t shortComplexSamples[][2],
                     double centerFrequency);
  void AppendSamples(fftwf_complex * floatComplexSamples,
                     double centerFrequency);
  bool GetNextSamples(fftwf_complex * outputBuffer, double & centerFrequency);
  void SetIsDone();
  bool GetIsDone();
};

