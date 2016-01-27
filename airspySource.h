#pragma once

#include <libairspy/airspy.h>

class AirspySource : public SignalSource
{
  enum StreamingState {
    Illegal = 0,
    Streaming,
    Done
  } m_streamingState;
  struct airspy_device * m_dev;
  bool m_done_streaming;
  bool m_didRetune;
  fftwf_complex * m_buffer;
  uint32_t m_bufferIndex;
  void handle_error(int status, const char * format, ...);
  static int _airspy_rx_callback(airspy_transfer* transfer);
  int airspy_rx_callback(void *samples, int sample_count);
  double set_sample_rate( double rate );

 public:
  AirspySource(std::string args,
               uint32_t sampleRate, 
               uint32_t sampleCount, 
               double startFrequency, 
               double stopFrequency);
  virtual ~AirspySource();
  virtual bool GetNextSamples(SampleQueue * sampleQueue, double_t & centerFrequency);
  virtual bool StartStreaming(uint32_t numIterations, SampleQueue & sampleQueue);
  virtual bool Start();
  virtual void ThreadWorker();
  virtual double Retune(double frequency);
};

