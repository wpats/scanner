#pragma once

#include <uhd/usrp/multi_usrp.hpp>

class B210Source : public SignalSource
{
  uhd::usrp::multi_usrp::sptr m_usrp;
  uhd::rx_streamer::sptr m_rx_stream;
  double m_currentFrequency;
  double m_frequencyIncrement;
  bool m_verbose;

  double Retune();

 public:
  B210Source(std::string args,
             uint32_t sampleRate, 
             uint32_t sampleCount, 
             double startFrequency, 
             double stopFrequency);
  virtual ~B210Source();
  virtual bool GetNextSamples(int16_t sample_buffer[][2], double & centerFrequency);
};

