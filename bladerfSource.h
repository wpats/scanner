/* The RX and TX modules are configured independently for these parameters */

#pragma once

#include <libbladeRF.h>

struct module_config {
  bladerf_module module;
  unsigned int frequency;
  unsigned int bandwidth;
  unsigned int samplerate;
  /* Gains */
  bladerf_lna_gain rx_lna;
  int vga1;
  int vga2;
};

class BladerfSource : public SignalSource
{
  struct bladerf * m_dev;
  struct bladerf_quick_tune * m_quickTunes;
  int configure_module(struct bladerf *dev, struct module_config *c);
  bool populate_quick_tunes();
  void handle_error(struct bladerf * dev, int status, const char * format, ...);

 public:
  BladerfSource(std::string args,
                uint32_t sampleRate, 
                uint32_t sampleCount, 
                double startFrequency, 
                double stopFrequency);
  virtual ~BladerfSource();
  virtual bool GetNextSamples(SampleBuffer * sample_buffer, double_t & centerFrequency);
  virtual bool StartStreaming(uint32_t numIterations, SampleBuffer & sampleBuffer);
  virtual void ThreadWorker();
  virtual double Retune(double frequency);
};

