#pragma once

#include <mirsdrapi-rsp.h>

class SdrplaySource : public SignalSource
{
  enum StreamingState {
    Illegal = 0,
    GetSamples,
    GotSamples
  } m_streamingState;
  int16_t * m_sample_buffer_i;
  int16_t * m_sample_buffer_q;
  int m_samplesPerPacket;
  uint32_t m_firstSampleNum;
  uint32_t m_bufferSize;
  const char * errorToString(mir_sdr_ErrT code);
  void handle_error(mir_sdr_ErrT status, const char * format, ...);

 public:
  SdrplaySource(std::string args,
                uint32_t sampleRate, 
                uint32_t sampleCount, 
                double startFrequency, 
                double stopFrequency,
                uint32_t bandWidth);
  virtual ~SdrplaySource();
  virtual bool GetNextSamples(SampleQueue * sampleQueue, double_t & centerFrequency);
  virtual bool StartStreaming(uint32_t numIterations, SampleQueue & sampleQueue);
  virtual void ThreadWorker();
  virtual double Retune(double frequency);
};

