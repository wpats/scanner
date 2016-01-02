
#pragma once

#include <time.h>
#include <gnuradio/fft/window.h>
#include "fft.h"
#include "messageQueue.h"

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

 public:
  enum Mode {
    Illegal,
    TimeDomain,
    FrequencyDomain
  };
    
 private:
  bool process_fft(fftwf_complex * fft_data, 
                   uint32_t center_frequency);
  void WriteToFile(const char * fileName, fftwf_complex * data);
  void WriteSamplesToFile(uint32_t count, double centerFrequency);
  void WriteSamplesToFile(uint64_t sequenceId, double centerFrequency);
  std::string GenerateFileName(std::string fileNameBase, 
                               time_t startTime, 
                               double_t centerFrequency);
  bool DoTimeDomainThresholding(fftwf_complex * inputSamples, double centerFrequency);
  void ProcessWrite(bool doWrite, 
                    double centerFrequency,
                    uint64_t sequenceId);

  uint32_t m_sampleCount;
  uint32_t m_sampleRate;
  uint32_t m_enob;
  uint32_t m_fileCounter;
  uint32_t m_preTrigger;
  uint32_t m_postTrigger;
  uint64_t m_endSequenceId;
  bool m_correctDCOffset;
  bool m_dcIgnoreWindow;
  bool m_writeSamples;
  bool m_writing;
  Mode m_mode;
  std::string m_fileNameBase;
  float m_threshold;
  FFT m_fft;
  FFTWindow m_fftWindow;
  SampleQueue * m_sampleQueue;
  fftwf_complex * m_inputSamples;
  fftwf_complex * m_fftOutputBuffer;
 public:
  ProcessSamples(uint32_t numSamples, 
                 uint32_t sampleRate, 
                 uint32_t enob,
                 float threshold, 
                 gr::fft::window::win_type windowType,
                 bool correctDCOffset,
                 Mode mode,
                 std::string fileNameBase = "",
                 uint32_t dcIgnoreWindow = 0,
                 uint32_t preTrigger = 2,
                 uint32_t postTrigger = 4);
  ~ProcessSamples();
  void Run(int16_t sample_buffer[][2], uint32_t centerFrequency);
  void RecordSamples(SignalSource * signalSource,
                     uint64_t count,
                     double threshold);
  bool StartProcessing(SampleQueue & sampleQueue);
  bool m_writeData;
};

