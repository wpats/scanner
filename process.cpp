#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <iostream>
#include <limits>
#include <cassert>
#include <volk/volk.h>
#include "fft.h"
#include "sampleBuffer.h"
#include "signalSource.h"
#include "process.h"

FFTWindow::FFTWindow(gr::fft::window::win_type type, uint32_t numSamples)
  : m_type(type),
    m_numSamples(numSamples)
{
  this->m_windowVector = gr::fft::window::build(type, numSamples, 0.0);
  this->m_window = reinterpret_cast<float *>(fftwf_malloc(sizeof(float) * numSamples));
  memcpy(this->m_window, &this->m_windowVector[0], sizeof(float) * numSamples);
}

FFTWindow::~FFTWindow()
{
  fftwf_free(this->m_window);
}

void FFTWindow::apply(fftwf_complex * samples)
{
  volk_32fc_32f_multiply_32fc_a(reinterpret_cast<lv_32fc_t *>(samples), 
                                reinterpret_cast<lv_32fc_t *>(samples), 
                                this->m_window, 
                                this->m_numSamples);
}

void ProcessSamples::process_fft(fftwf_complex * fft_data, 
                                 uint32_t center_frequency)
{
  uint32_t start_frequency = center_frequency - this->m_sampleRate/2;
  uint32_t bin_step = this->m_sampleRate/this->m_sampleCount;
  float magnitudes[this->m_sampleCount];

  Utility::complex_to_magnitude(fft_data, magnitudes, this->m_sampleCount);

  for (uint32_t i = 0; i < this->m_sampleCount; i++) {
    uint32_t j = (i + this->m_sampleCount/2) % this->m_sampleCount;
    if (j < this->m_dcIgnoreWindow || (this->m_sampleCount - j < this->m_dcIgnoreWindow)) {
      continue;
    }
    if (magnitudes[j] > this->m_threshold) {
      uint32_t frequency = start_frequency + i*bin_step;
      printf("freq %u power_db %f\n", frequency, magnitudes[j]);
    }
  }
}

ProcessSamples::ProcessSamples(uint32_t numSamples, 
                               uint32_t sampleRate, 
                               uint32_t enob,
                               float threshold,
                               gr::fft::window::win_type windowType,
                               bool correctDCOffset,
                               Mode mode,
                               std::string fileNameBase,
                               uint32_t dcIgnoreWindow)
  : m_sampleCount(numSamples),
    m_sampleRate(sampleRate),
    m_enob(enob),
    m_fileCounter(0),
    m_threshold(threshold),
    m_fftWindow(windowType, numSamples),
    m_correctDCOffset(correctDCOffset),
    m_dcIgnoreWindow(dcIgnoreWindow),
    m_fft(numSamples),
    m_mode(mode),
    m_sampleBuffer(nullptr),
    m_fileNameBase(fileNameBase)
{
  assert(mode > Illegal && mode <= FrequencyDomain);
  m_inputSamples = reinterpret_cast<fftwf_complex *>(fftwf_alloc_complex(numSamples));
  m_fftOutputBuffer = reinterpret_cast<fftwf_complex *>(fftwf_alloc_complex(numSamples));
}

ProcessSamples::~ProcessSamples()
{
  fftwf_free(this->m_inputSamples);
  fftwf_free(this->m_fftOutputBuffer);
}

void ProcessSamples::WriteToFile(const char * fileName, fftwf_complex * data)
{
  FILE * outFile = fopen(fileName, "w");
  if (outFile == NULL) {
    fprintf(stderr, "Failed to open file '%s'\n", fileName);
    return;
  }
  for (uint32_t i = 0; i < this->m_sampleCount; i++) {
    fprintf(outFile, "%f, %f\n", double(data[i][0]), double(data[i][1]));
  }
  fclose(outFile);
}

void ProcessSamples::Run(int16_t sample_buffer[][2], uint32_t centerFrequency)
{
  Utility::short_complex_to_float_complex(sample_buffer, 
                                          this->m_inputSamples,
                                          this->m_sampleCount,
                                          this->m_enob,
                                          this->m_correctDCOffset);
  if (this->m_mode == FrequencyDomain) {
    this->m_fftWindow.apply(this->m_inputSamples);
    this->m_fft.process(this->m_fftOutputBuffer, this->m_inputSamples);
    this->process_fft(this->m_fftOutputBuffer, centerFrequency);
  }
}

std::string ProcessSamples::GenerateFileName(std::string fileNameBase, 
                                             time_t startTime, 
                                             double_t centerFrequency)
{
  char buffer[256];
  const char timeformat[] = "-%Y%m%d-%T";
  struct tm * timeStruct = localtime(&startTime);
  if (timeStruct == nullptr) {
    perror("localtime");
    exit(1);
  }
  strcpy(buffer, fileNameBase.c_str());
  uint32_t length = strlen(buffer);
  if (strftime(buffer+length, sizeof(buffer)-length, timeformat, timeStruct) == 0) {
    fprintf(stderr, "strftime returned 0");
    exit(1);
  }
  sprintf(buffer+strlen(buffer), "-%.0f-%u", centerFrequency, ++this->m_fileCounter);
  return std::string(buffer);
}

void ProcessSamples::WriteSamplesToFile(uint32_t count, double centerFrequency)
{
  assert(this->m_sampleBuffer != nullptr);
  time_t startTime;
  startTime = time(NULL);
  std::string fileName = this->GenerateFileName(this->m_fileNameBase, startTime, centerFrequency);
  this->m_sampleBuffer->WriteSamplesToFile(fileName, count);
}

void ProcessSamples::RecordSamples(SignalSource * signalSource, 
                                   uint64_t count,
                                   double threshold)
{
  int16_t sample_buffer[this->m_sampleCount][2];
  double_t centerFrequency;
  time_t startTime;
  for (uint64_t i = 0; i < count; i += this->m_sampleCount) {
    startTime = time(NULL);
    Utility::short_complex_to_float_complex(sample_buffer, 
                                            this->m_inputSamples,
                                            this->m_sampleCount,
                                            this->m_enob,
                                            this->m_correctDCOffset);
    // Check for threshold.
    std::string fileName = this->GenerateFileName(this->m_fileNameBase, startTime, centerFrequency);
    // this->WriteSamplesToFile(fileName, this->m_sampleCount);
   }
}

void ProcessSamples::DoTimeDomainThresholding(fftwf_complex * inputSamples, 
                                              double centerFrequency)
{
  double log10 = log2(10);
  for (uint32_t i = 0; i < this->m_sampleCount; i++) {
    float re = inputSamples[i][0];
    float im = inputSamples[i][1];
    float mag = sqrt(re * re + im * im); // / this->m_sampleCount;
    float magnitude = 10 * log2(mag) / log10;
    if (magnitude > this->m_threshold) {
      printf("Signal %f above threshold %f at frequency %.0f\n", 
             magnitude, 
             this->m_threshold,
             centerFrequency);
      this->WriteSamplesToFile(4 * this->m_sampleCount, centerFrequency);
      return;
    }
  }
}

bool ProcessSamples::StartProcessing(SampleBuffer & sampleBuffer)
{
  double centerFrequency;
  uint32_t i = 0;
  this->m_sampleBuffer = &sampleBuffer;
  while (sampleBuffer.GetNextSamples(this->m_inputSamples, centerFrequency)) {
    // printf("Processing %d\n", i++);
    if (this->m_mode == TimeDomain) {
      this->DoTimeDomainThresholding(this->m_inputSamples, centerFrequency);
    } else if (this->m_mode == FrequencyDomain) {
      this->m_fftWindow.apply(this->m_inputSamples);
      this->m_fft.process(this->m_fftOutputBuffer, this->m_inputSamples);
      this->process_fft(this->m_fftOutputBuffer, centerFrequency);
    }
  }
  return true;
}

void Utility::short_complex_to_float_complex(int16_t * realSamples,
                                             int16_t * imagSamples,
                                             fftwf_complex * destination,
                                             uint32_t sampleCount,
                                             uint32_t enob,
                                             bool correctDCOffset)
{
  int16_t max = 1 << (enob - 1);
  float onebymax = float(1.0/max);
  int32_t dc_real = 0;
  int32_t dc_imag = 0;
  if (correctDCOffset) {
    for (uint32_t i = 0; i < sampleCount; i++) {
      dc_real += realSamples[i];
      dc_imag += imagSamples[i];
    }
    dc_real /= sampleCount;
    dc_imag /= sampleCount;
  }
  for (uint32_t i = 0; i < sampleCount; i++) {
    destination[i][0] = float(realSamples[i] - dc_real)*onebymax;
    destination[i][1] = float(imagSamples[i] - dc_imag)*onebymax;
  }
}

void Utility::short_complex_to_float_complex(int16_t source[][2],
                                             fftwf_complex * destination,
                                             uint32_t sampleCount,
                                             uint32_t enob,
                                             bool correctDCOffset)
{
  int16_t max = 1 << (enob - 1);
  float onebymax = float(1.0/max);
  int32_t dc_real = 0;
  int32_t dc_imag = 0;
  int16_t max_r = -1;
  int16_t max_i = -1;
  if (correctDCOffset) {
    for (uint32_t i = 0; i < sampleCount; i++) {
      dc_real += source[i][0];
      dc_imag += source[i][1];
      max_r = std::max<int16_t>(max_r, source[i][0]);
      max_i = std::max<int16_t>(max_i, source[i][1]);
    }
    dc_real /= sampleCount;
    dc_imag /= sampleCount;
  }
  for (uint32_t i = 0; i < sampleCount; i++) {
    destination[i][0] = float(source[i][0] - dc_real)*onebymax;
    destination[i][1] = float(source[i][1] - dc_imag)*onebymax;
  }
}

void Utility::complex_to_magnitude(fftwf_complex * fft_data, 
                                   float * magnitudes,
                                   uint32_t sampleCount)
{
  double log10 = log2(10);
  for (uint32_t i = 0; i < sampleCount; i++) {
    float re = fft_data[i][0];
    float im = fft_data[i][1];
    float mag = sqrt(re * re + im * im) / sampleCount;
    magnitudes[i] = 10 * log2(mag) / log10;
  }
}

