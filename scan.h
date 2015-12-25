#pragma once

int populate_quick_tunes(struct bladerf * dev,
                         uint32_t num_frequencies,
                         const unsigned int frequencies[],
                         struct bladerf_quick_tune quick_tunes[]);

void handle_error(struct bladerf * dev, int status, const char * format);


void short_complex_to_float_complex(int16_t source[][2],
                                    uint32_t sample_count,
                                    fftwf_complex * destination);

void complex_to_magnitude(fftwf_complex * fft_data, 
                          uint32_t size,
                          float * magnitudes);
void process_fft(fftwf_complex * fft_data, 
                 uint32_t size, 
                 uint32_t center_frequency,
                 uint32_t sample_rate);
