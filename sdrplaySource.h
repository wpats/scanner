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
  const char * errorToString(mir_sdr_ErrT code);
  void handle_error(mir_sdr_ErrT status, const char * format, ...);

 public:
  SdrplaySource(std::string args,
               uint32_t sampleRate, 
               uint32_t sampleCount, 
               double startFrequency, 
               double stopFrequency);
  virtual ~SdrplaySource();
  virtual bool GetNextSamples(int16_t sample_buffer[][2], double & centerFrequency);
};

