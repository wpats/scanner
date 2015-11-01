#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <iostream>
#include <limits>
#include "fft.h"
#include "process.h"

void ProcessSamples::short_complex_to_float_complex(int16_t source[][2],
                                                    fftwf_complex * destination)
{
  int16_t max = 2048;
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
    if (j == 0) continue;
    if (magnitudes[j] > this->m_threshold) {
      uint32_t frequency = start_frequency + i*bin_step;
      printf("freq %u power_db %f\n", frequency, magnitudes[j]);
    }
  }
}

ProcessSamples::ProcessSamples(uint32_t numSamples, 
                               uint32_t sampleRate, 
                               float threshold,
                               bool correctDCOffset)
  : m_sampleCount(numSamples),
    m_sampleRate(sampleRate),
    m_threshold(threshold),
    m_correctDCOffset(correctDCOffset),
    m_fft(numSamples)
{
  m_inputSamples = reinterpret_cast<fftwf_complex *>(fftwf_alloc_complex(numSamples));
  m_fftOutputBuffer = reinterpret_cast<fftwf_complex *>(fftwf_alloc_complex(numSamples));
}

void ProcessSamples::Run(int16_t sample_buffer[][2], uint32_t centerFrequency)
{
  this->short_complex_to_float_complex(sample_buffer, this->m_inputSamples);
  this->m_fft.process(this->m_fftOutputBuffer, this->m_inputSamples);
  this->process_fft(this->m_fftOutputBuffer, centerFrequency);
}
 
