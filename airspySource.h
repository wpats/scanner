#pragma once

#include <libairspy/airspy.h>

class AirspySource : public SignalSource
{
  enum StreamingState {
    Illegal = 0,
    GetSamples,
    GotSamples
  } m_streamingState;
  struct airspy_device * m_dev;
  int16_t (*m_sample_buffer)[2];
  bool m_done_streaming;
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
  virtual bool GetNextSamples(int16_t sample_buffer[][2], double & centerFrequency);
};

