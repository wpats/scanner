
#pragma once

#include <gnuradio/fft/window.h>

class FFTWindow {
  void ComputeWindowHanning();
  void ComputeWindowHamming();
  void ComputeWindowBlackman();
  void ComputeWindowBlackmanHarris();
  void ComputeWindowDolphChebyshev();
  
  std::vector<float> m_windowVector;
  float * m_window;
  uint32_t m_numSamples;
 public:
  enum WindowKind {
    WindowNone,
    WindowHanning,
    WindowHamming,
    WindowBlackman,
    WindowBlackmanHarris,
    WindowDolphChebyshev
  } m_kind;
  gr::fft::window::win_type m_type;
  FFTWindow(WindowKind kind, uint32_t numSamples);
  FFTWindow(gr::fft::window::win_type type, uint32_t numSamples);
  ~FFTWindow();
  void apply(fftwf_complex * samples);
};


class ProcessSamples {

  void short_complex_to_float_complex(int16_t source[][2],
                                      fftwf_complex * destination);
  void complex_to_magnitude(fftwf_complex * fft_data, 
                            float * magnitudes);
  void process_fft(fftwf_complex * fft_data, 
                   uint32_t center_frequency);
  void WriteToFile(const char * fileName, fftwf_complex * data);

  uint32_t m_sampleCount;
  uint32_t m_sampleRate;
  uint32_t m_enob;
  bool m_correctDCOffset;
  bool m_ignoreCenterFrequency;
  float m_threshold;
  FFT m_fft;
  FFTWindow m_fftWindow;
  fftwf_complex * m_inputSamples;
  fftwf_complex * m_fftOutputBuffer;
  
 public:
  ProcessSamples(uint32_t numSamples, 
                 uint32_t sampleRate, 
                 uint32_t enob,
                 float threshold, 
                 gr::fft::window::win_type windowType,
                 bool correctDCOffset,
                 bool ignoreCenterFrequency);
  void Run(int16_t sample_buffer[][2], uint32_t centerFrequency);
  bool m_writeData;
};
