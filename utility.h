#pragma once

class Utility
{
 public:
  static void short_complex_to_float_complex(int16_t * realSamples,
                                             int16_t * complexSamples,
                                             fftwf_complex * destination,
                                             uint32_t sampleCount,
                                             uint32_t enob,
                                             bool correctDCOffset);
  static void byte_complex_to_float_complex(int8_t source[][2],
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
