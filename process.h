
class ProcessSamples {

  void short_complex_to_float_complex(int16_t source[][2],
                                      fftwf_complex * destination);
  void complex_to_magnitude(fftwf_complex * fft_data, 
                            float * magnitudes);
  void process_fft(fftwf_complex * fft_data, 
                   uint32_t center_frequency);

  uint32_t m_sampleCount;
  uint32_t m_sampleRate;
  bool m_correctDCOffset;
  float m_threshold;
  FFT m_fft;
  fftwf_complex * m_inputSamples;
  fftwf_complex * m_fftOutputBuffer;
  
 public:
  ProcessSamples(uint32_t numSamples, uint32_t sampleRate, float threshold, bool correctDCOffset);
  void Run(int16_t sample_buffer[][2], uint32_t centerFrequency);
};
