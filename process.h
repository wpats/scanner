
#pragma once

#include <time.h>
#include <gnuradio/fft/window.h>
#include "fft.h"
#include "buffer.h"

class SampleBuffer;
class SignalSource;

class FFTWindow {
  std::vector<float> m_windowVector;
  float * m_window;
  uint32_t m_numSamples;
 public:
  gr::fft::window::win_type m_type;
  FFTWindow(gr::fft::window::win_type type, uint32_t numSamples);
  ~FFTWindow();
  void apply(fftwf_complex * samples);
};


class ProcessSamples {

  void process_fft(fftwf_complex * fft_data, 
                   uint32_t center_frequency);
  void WriteToFile(const char * fileName, fftwf_complex * data);
  void WriteSamplesToFile(std::string fileName, uint32_t count);
  std::string GenerateFileName(time_t startTime, double_t centerFrequency);

  uint32_t m_sampleCount;
  uint32_t m_sampleRate;
  uint32_t m_enob;
  uint32_t m_fileCounter;
  bool m_correctDCOffset;
  bool m_dcIgnoreWindow;
  bool m_writeSamples;
  bool m_doFFT;
  float m_threshold;
  FFT m_fft;
  FFTWindow m_fftWindow;
  SampleBuffer * m_sampleBuffer;
  fftwf_complex * m_inputSamples;
  fftwf_complex * m_fftOutputBuffer;
 public:
  ProcessSamples(uint32_t numSamples, 
                 uint32_t sampleRate, 
                 uint32_t enob,
                 float threshold, 
                 gr::fft::window::win_type windowType,
                 bool correctDCOffset,
                 uint32_t dcIgnoreWindow = 0,
                 bool doFFT = true);
  ~ProcessSamples();
  void Run(int16_t sample_buffer[][2], uint32_t centerFrequency);
  void RecordSamples(SignalSource * signalSource,
                     uint64_t count,
                     double threshold);
  bool StartProcessing(SampleBuffer & sampleBuffer);
  bool m_writeData;
};

class Utility
{
 public:
  static void short_complex_to_float_complex(int16_t * realSamples,
                                             int16_t * complexSamples,
                                             fftwf_complex * destination,
                                             uint32_t sampleCount,
                                             uint32_t enob,
                                             bool correctDCOffset);
  static void short_complex_to_float_complex(int16_t source[][2],
                                             fftwf_complex * destination,
                                             uint32_t sampleCount,
                                             uint32_t enob,
                                             bool correctDCOffset);

  static void complex_to_magnitude(fftwf_complex * fft_data, 
                                   float * magnitudes,
                                   uint32_t sampleCount);
};
