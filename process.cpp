#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <iostream>
#include <limits>
#include <cassert>
#include <volk/volk.h>
#include "fft.h"
#include "process.h"


FFTWindow::FFTWindow(WindowKind kind, uint32_t numSamples)
  : m_kind(kind),
    m_numSamples(numSamples)
{
#if 0
  this->m_window = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * numSamples);
  memset(this->m_window, 0, sizeof(fftwf_complex) * numSamples);
  switch (kind) {
  case WindowNone:
    break;
  case WindowHanning:
    this->ComputeWindowHanning();
    break;
  case WindowHamming:
    this->ComputeWindowHamming();
    break;
  case WindowBlackman:
    this->ComputeWindowBlackman();
    break;
  case WindowBlackmanHarris:
    this->ComputeWindowBlackmanHarris();
    break;
  case WindowDolphChebyshev:
    this->ComputeWindowDolphChebyshev();
  default:
    assert(false, "Unknown window kind");
  }
#endif
}

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

#if 0

void FFTWindow::ComputeWindowHanning()
{
  for (uint32_t i = 0; i < this->m_numSamples; i++) {
    this->m_window[i] = 0.5 * (1.0 - cos(2 * M_PI * (i + 1)/(this->m_numSamples + 1)));
  }
}

void FFTWindow::ComputeWindowHamming()
{
  for (uint32_t i = 0; i < this->m_numSamples; i++) {
    this->m_window[i] = 0.54 - 0.46 * (cos(2 * M_PI * i/(this->m_numSamples - 1)));
  }
}

void FFTWindow::ComputeWindowBlackman()
{
  uint32_t N = this->m_numSamples / 3;
  uint32_t No2 = (N-1)/2;
  fftwf_complex matrix[3][N];
  memset(matrix, 0, sizeof(fftwf_complex) * this->m_numSamples * 3);
  for (uint32_t l = 0; l < 2; l++) {
    for (uint32_t i = 0; i < N; i++) {
      uint32_t n = i - No2;
      matrix[l][i] = cos(l * 2 * M_PI * n/N);
    }
  }
  for (uint32_t i = N; i < 2*N; i++) {
    this->m_window[i] += 0.4 * matrix[0][i-N] + 0.5 * matrix[1][i-N]
      + 0.08 * matrix[2][i-N];
  }
}

void FFTWindow::ComputeWindowBlackmanHarris()
{
}

void FFTWindow::ComputeWindowDolphChebyshev()
{
}
#endif


void ProcessSamples::short_complex_to_float_complex(int16_t source[][2],
                                                    fftwf_complex * destination)
{
  int16_t max = 1 << (this->m_enob - 1);
  int32_t dc_real = 0;
  int32_t dc_imag = 0;
  if (this->m_correctDCOffset) {
    for (uint32_t i = 0; i < this->m_sampleCount; i++) {
      dc_real += source[i][0];
      dc_imag += source[i][1];
    }
    dc_real /= this->m_sampleCount;
    dc_imag /= this->m_sampleCount;
  }
  for (uint32_t i = 0; i < this->m_sampleCount; i++) {
    destination[i][0] = float(source[i][0] - dc_real)*(1.0/max);
    destination[i][1] = float(source[i][1] - dc_imag)*(1.0/max);
  }
}


void ProcessSamples::complex_to_magnitude(fftwf_complex * fft_data, 
                                          float * magnitudes)
{
  for (uint32_t i = 0; i < this->m_sampleCount; i++) {
    float re = fft_data[i][0];
    float im = fft_data[i][1];
    float mag = sqrt(re * re + im * im) / this->m_sampleCount;
    magnitudes[i] = 10 * log2(mag) / log2(10);
  }
}

void ProcessSamples::process_fft(fftwf_complex * fft_data, 
                                 uint32_t center_frequency)
{
  uint32_t start_frequency = center_frequency - this->m_sampleRate/2;
  uint32_t bin_step = this->m_sampleRate/this->m_sampleCount;
  float magnitudes[this->m_sampleCount];

  complex_to_magnitude(fft_data, magnitudes);

  for (uint32_t i = 0; i < this->m_sampleCount; i++) {
    uint32_t j = (i + this->m_sampleCount/2) % this->m_sampleCount;
    if ((j <= 5 || (this->m_sampleCount - j <= 5)) && this->m_ignoreCenterFrequency) continue;
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
                               bool ignoreCenterFrequency)
  : m_sampleCount(numSamples),
    m_sampleRate(sampleRate),
    m_enob(enob),
    m_threshold(threshold),
    m_fftWindow(windowType, numSamples),
    m_correctDCOffset(correctDCOffset),
    m_ignoreCenterFrequency(ignoreCenterFrequency),
    m_fft(numSamples),
    m_writeData(false)
{
  m_inputSamples = reinterpret_cast<fftwf_complex *>(fftwf_alloc_complex(numSamples));
  m_fftOutputBuffer = reinterpret_cast<fftwf_complex *>(fftwf_alloc_complex(numSamples));
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
  this->short_complex_to_float_complex(sample_buffer, this->m_inputSamples);
  if (this->m_writeData && centerFrequency == 762000000) {
    this->WriteToFile("samples.txt", this->m_inputSamples);
  }
  this->m_fftWindow.apply(this->m_inputSamples);
  if (this->m_writeData && centerFrequency == 762000000) {
    this->WriteToFile("windowSamples.txt", this->m_inputSamples);
  }
  this->m_fft.process(this->m_fftOutputBuffer, this->m_inputSamples);
  if (this->m_writeData && centerFrequency == 762000000) {
    this->WriteToFile("fft.txt", this->m_fftOutputBuffer);
    this->m_writeData = false;
  }
  this->process_fft(this->m_fftOutputBuffer, centerFrequency);
}
 
