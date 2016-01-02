#include <stdlib.h>
#include <limits>
#include <cassert>
#include <cstdint>
#include <algorithm>
#include "fft.h"
#include "utility.h"

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

