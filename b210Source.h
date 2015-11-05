
class B210Source : public SignalSource
{
  uint32_t m_sampleRate;
  uint32_t m_sampleCount;
  uhd::usrp::multi_usrp::sptr m_usrp;
  uhd::rx_streamer::sptr m_rx_stream;
  double m_currentFrequency;
  double m_startFrequency;
  double m_stopFrequency;
  double m_frequencyIncrement;
  bool m_verbose;

  void Retune();
  void UpdateCurrentFrequency();

 public:
  B210Source(std::string args,
             uint32_t sampleRate, 
             uint32_t sampleCount, 
             double startFrequency, 
             double stopFrequency);
  virtual ~B210Source();
  virtual bool GetNextSamples(int16_t sample_buffer[][2], double & centerFrequency);
};

