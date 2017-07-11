#pragma once

#include <libhackrf/hackrf.h>

class HackRFSource : public SignalSource
{
  enum StreamingState {
    Illegal = 0,
    Streaming,
    DoRetune,
    Done
  };
  std::atomic<StreamingState> m_streamingState;
  struct timeval m_nextValidStreamTime;
  double m_retuneTime;
  uint32_t m_dropPacketCount;
  uint32_t m_scanStartCount;
  double m_centerFrequency;
  // New hackrf sweep parameters.
  uint16_t m_scanStartFrequency;
  uint16_t m_scanStopFrequency;
  uint32_t m_scanNumBytes;
  uint32_t m_scanStepWidth;
  uint32_t m_scanOffset;
  bool m_isScanStart;
  struct hackrf_device * m_dev;
  bool m_done_streaming;
  bool m_didRetune;
  void handle_error(int status, const char * format, ...);
  static int _hackRF_rx_callback(hackrf_transfer* transfer);
  int hackRF_rx_callback(hackrf_transfer* transfer);
  double set_sample_rate( double rate );
  double interpolateSamples(hackrf_transfer * transfer);
 public:
  HackRFSource(std::string args,
               uint32_t sampleRate, 
               uint32_t sampleCount, 
               double startFrequency, 
               double stopFrequency);
  virtual ~HackRFSource();
  virtual bool GetNextSamples(SampleQueue * sampleQueue, double_t & centerFrequency);
  virtual bool StartStreaming(uint32_t numIterations, SampleQueue & sampleQueue);
  virtual bool Start();
  virtual void ThreadWorker();
  virtual double Retune(double frequency);
};

