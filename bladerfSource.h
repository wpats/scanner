/* The RX and TX modules are configured independently for these parameters */
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
  uint32_t m_sampleRate;
  uint32_t m_sampleCount;
  struct bladerf * m_dev;
  uint32_t m_numFrequencies;
  uint32_t * m_frequencies;
  struct bladerf_quick_tune * m_quickTunes;
  uint32_t m_frequencyIndex;
  int configure_module(struct bladerf *dev, struct module_config *c);
  bool populate_quick_tunes();
  void handle_error(struct bladerf * dev, int status, const char * format, ...);

 public:
  BladerfSource(uint32_t sampleRate, 
                uint32_t sampleCount, 
                double startFrequency, 
                double stopFrequency);
  virtual ~BladerfSource();
  virtual bool GetNextSamples(int16_t sample_buffer[][2], double & centerFrequency);
};

