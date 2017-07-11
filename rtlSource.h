#pragma once

#include <rtl-sdr.h>

class RtlSource : public SignalSource
{
  enum StreamingState {
    Illegal = 0,
    Streaming,
    Done
  } m_streamingState;
  rtlsdr_dev_t * m_dev;
  double m_retuneTime;
  uint32_t m_dropPacketCount;
  uint32_t m_dropPacketValue;
  int8_t (*m_buffer)[2];
  uint32_t m_bufferIndex;
  void handle_error(int status, const char * format, ...);
  static void _rtl_rx_callback(unsigned char *buf, uint32_t len, void *ctx);  
  int rtl_rx_callback(void *samples, int sample_count);
  double set_sample_rate( double rate );

 public:
  RtlSource(std::string args,
               uint32_t sampleRate, 
               uint32_t sampleCount, 
               double startFrequency, 
               double stopFrequency);
  virtual ~RtlSource();
  virtual bool GetNextSamples(SampleQueue * sampleQueue, double_t & centerFrequency);
  virtual bool StartStreaming(uint32_t numIterations, SampleQueue & sampleQueue);
  virtual bool Start();
  virtual void ThreadWorker();
  virtual double Retune(double frequency);
};

