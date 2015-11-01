#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <libbladeRF.h>
#include <math.h>
#include <iostream>
#include <limits>
#include <boost/program_options.hpp>
#include "fft.h"
#include "process.h"
#include "scan.h"

/* The RX and TX modules are configured independently for these parameters */
struct module_config {
  bladerf_module module;
  unsigned int frequency;
  unsigned int bandwidth;
  unsigned int samplerate;
  /* Gains */
  bladerf_lna_gain rx_lna;
  int vga1;
  int vga2;
};

int configure_module(struct bladerf *dev, struct module_config *c)
{
  int status;
  status = bladerf_set_frequency(dev, c->module, c->frequency);
  if (status != 0) {
    fprintf(stderr, "Failed to set frequency = %u: %s\n",
            c->frequency, bladerf_strerror(status));
    return status;
  }
  status = bladerf_set_sample_rate(dev, c->module, c->samplerate, NULL);
  if (status != 0) {
    fprintf(stderr, "Failed to set samplerate = %u: %s\n",
            c->samplerate, bladerf_strerror(status));
    return status;
  }
  status = bladerf_set_bandwidth(dev, c->module, c->bandwidth, NULL);
  if (status != 0) {
    fprintf(stderr, "Failed to set bandwidth = %u: %s\n",
            c->bandwidth, bladerf_strerror(status));
    return status;
  }
  switch (c->module) {
  case BLADERF_MODULE_RX:
    /* Configure the gains of the RX LNA, RX VGA1, and RX VGA2 */
    status = bladerf_set_lna_gain(dev, c->rx_lna);
    if (status != 0) {
      fprintf(stderr, "Failed to set RX LNA gain: %s\n",
              bladerf_strerror(status));
      return status;
    }
    status = bladerf_set_rxvga1(dev, c->vga1);
    if (status != 0) {
      fprintf(stderr, "Failed to set RX VGA1 gain: %s\n",
              bladerf_strerror(status));
      return status;
    }
    status = bladerf_set_rxvga2(dev, c->vga2);
    if (status != 0) {
      fprintf(stderr, "Failed to set RX VGA2 gain: %s\n",
              bladerf_strerror(status));
      return status;
    }
    break;
  case BLADERF_MODULE_TX:
    /* Configure the TX VGA1 and TX VGA2 gains */
    status = bladerf_set_txvga1(dev, c->vga1);
    if (status != 0) {
      fprintf(stderr, "Failed to set TX VGA1 gain: %s\n",
              bladerf_strerror(status));
      return status;
    }
    status = bladerf_set_txvga2(dev, c->vga2);
    if (status != 0) {
      fprintf(stderr, "Failed to set TX VGA2 gain: %s\n",
              bladerf_strerror(status));
      return status;
    }
    break;
  default:
    status = BLADERF_ERR_INVAL;
    fprintf(stderr, "%s: Invalid module specified (%d)\n",
            __FUNCTION__, c->module);
  }
  return status;
}

void handle_error(struct bladerf * dev, int status, const char * format)
{
  if (status != 0) {
    fprintf(stderr, format, bladerf_strerror(status));
    bladerf_close(dev);
    exit(1);
  }
}

float threshold;

/* Usage:
 * libbladeRF_example_boilerplate [serial #]
 *
 * If a serial number is supplied, the program will attempt to open the
 * device with the provided serial number.
 *
 * Otherwise, the first available device will be used.
 */
int main(int argc, char *argv[])
{
  int status;
  struct module_config config;
  struct bladerf *dev = NULL;
  struct bladerf_devinfo dev_info;
  uint32_t sample_rate = 8000000;
  uint32_t num_iterations;
  namespace po = boost::program_options;

  po::options_description desc("Program options");
  desc.add_options()
    ("help", "print help message")
    ("n_iterations,n", po::value<uint32_t>(&num_iterations)->default_value(10), "Number of iterations")
    ("sample_rate,s", po::value<uint32_t>(&sample_rate)->default_value(8000000), "Sample rate")
    ("threshold,t", po::value<float>(&threshold)->default_value(10.0), "Threshold");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 1;
  }
    
  /* Initialize the information used to identify the desired device
   * to all wildcard (i.e., "any device") values */
  bladerf_init_devinfo(&dev_info);
  /* Request a device with the provided serial number.
   * Invalid strings should simply fail to match a device. */
  // if (argc >= 2) {
  //  strncpy(dev_info.serial, argv[1], sizeof(dev_info.serial) - 1);
  //}
  status = bladerf_open_with_devinfo(&dev, &dev_info);
  HANDLE_ERROR("Unable to open device: %s\n");

  /* Set up RX module parameters */
  config.module = BLADERF_MODULE_RX;
  config.frequency = 619000000;
  config.bandwidth = sample_rate;
  config.samplerate = sample_rate;
  config.rx_lna = BLADERF_LNA_GAIN_MAX;
  config.vga1 = 30;
  config.vga2 = 3;
  status = configure_module(dev, &config);
  HANDLE_ERROR("Failed to configure RX module. Exiting.\n");

  /* Set up TX module parameters */
  config.module = BLADERF_MODULE_TX;
  config.frequency = 918000000;
  config.bandwidth = 1500000;
  config.samplerate = 250000;
  config.vga1 = -14;
  config.vga2 = 0;
  status = configure_module(dev, &config);
  HANDLE_ERROR("Failed to configure TX module. Exiting.\n");

  /* Application code goes here.
   *
   * Don't forget to call bladerf_enable_module() before attempting to
   * transmit or receive samples!
   */
  
  bladerf_enable_module(dev, BLADERF_MODULE_RX, true);
  bladerf_enable_module(dev, BLADERF_MODULE_TX, false);

  const uint32_t num_samples = 8192;
  status = bladerf_sync_config(dev,
                               BLADERF_MODULE_RX,
                               BLADERF_FORMAT_SC16_Q11,
                               8,
                               num_samples,
                               4,
                               5);
  
  const uint32_t num_frequencies = (3800000000 - 300000000)/sample_rate + 1;
  uint32_t frequencies[num_frequencies];
  struct bladerf_quick_tune quick_tunes[num_frequencies];
  int16_t sample_buffer[num_samples][2];
  char format[128];

  for (uint32_t i = 0; i < num_frequencies; i++) {
    frequencies[i] = 300000000 + i * sample_rate;
#if 0
    fprintf(stderr, "Frequency %d: %u\n", i, frequencies[i]);
#endif
  }

  populate_quick_tunes(dev, num_frequencies, frequencies, quick_tunes);

  ProcessSamples process(num_samples, sample_rate, threshold, true);

  struct timespec start, stop;
  clock_gettime(CLOCK_REALTIME, &start);

  for (uint32_t i = 0; i < num_iterations; i++) {
    for (uint32_t j = 0; j < num_frequencies; j++) {
      /* Tune to the specified frequency immediately via BLADERF_RETUNE_NOW.
       *
       * Alternatively, this re-tune could be scheduled by providing a
       * timestamp counter value */
      uint32_t delta = 100;
      status = bladerf_schedule_retune(dev, 
                                       BLADERF_MODULE_RX, 
                                       BLADERF_RETUNE_NOW, // + delta, 
                                       0,
                                       &quick_tunes[j]);
      if (status != 0) {
        sprintf(format, "Failed to apply quick tune at %u:%u Hz: %%s\n", 
                i,
                frequencies[j]);
        HANDLE_ERROR(format);
      }

      // sleep(0.010);
      //fprintf(stderr, "Tuned to %u\n", frequencies[j]);
      /* ... Handle signals at current frequency ... */
      struct bladerf_metadata metadata;
      status = bladerf_sync_rx(dev,
                               sample_buffer,
                               num_samples,
                               &metadata,
                               0);
                             
      if (status != 0) {
        sprintf(format, "Failed to receive samples at %u:%u Hz: %%s\n", 
                i,
                frequencies[j]);
        HANDLE_ERROR(format);
      }

      process.Run(sample_buffer, frequencies[j]);
    }
  }
  // Calculate and report time.
  clock_gettime(CLOCK_REALTIME, &stop);
  double startd = start.tv_sec*1000.0 + start.tv_nsec/1e6;
  double stopd = stop.tv_sec*1000.0 + stop.tv_nsec/1e6;
  double elapsed = stopd - startd;
  fprintf(stderr, "Elapsed time = %f ms\n", elapsed);

  bladerf_close(dev);
  return 0;
}

int populate_quick_tunes(struct bladerf * dev,
                         uint32_t num_frequencies,
                         const uint32_t frequencies[],
                         struct bladerf_quick_tune quick_tunes[])
{
  int status;
  unsigned int i, j;
  char format[128];
  /* Get our quick tune parameters for each frequency we'll be using */
  for (i = 0; i < num_frequencies; i++) {
    status = bladerf_set_frequency(dev, BLADERF_MODULE_RX, frequencies[i]);
    if (status != 0) {
      sprintf(format, "Failed to set frequency to %u Hz: %%s\n", frequencies[i]);
      HANDLE_ERROR(format);
    }
    status = bladerf_get_quick_tune(dev, BLADERF_MODULE_RX, &quick_tunes[i]);
    if (status != 0) {
      sprintf(format, "Failed to get quick tune for %u Hz: %%s\n",
              frequencies[i]);
    }
  }
}


void short_complex_to_float_complex(int16_t source[][2],
                                    uint32_t sample_count,
                                    fftwf_complex * destination)
{
  int16_t max = 2048;
  int32_t dc_real = 0;
  int32_t dc_imag = 0;
  for (uint32_t i = 0; i < sample_count; i++) {
    dc_real += source[i][0];
    dc_imag += source[i][1];
  }
  dc_real /= sample_count;
  dc_imag /= sample_count;
  for (uint32_t i = 0; i < sample_count; i++) {
    destination[i][0] = float(source[i][0] - dc_real)*(1.0/max);
    destination[i][1] = float(source[i][1] - dc_imag)*(1.0/max);
  }
}

void complex_to_magnitude(fftwf_complex * fft_data, 
                          uint32_t size,
                          float * magnitudes)
{
  for (uint32_t i = 0; i < size; i++) {
    float re = fft_data[i][0];
    float im = fft_data[i][1];
    float mag = sqrt(re * re + im * im) / size;
    magnitudes[i] = 10 * log2(mag) / log2(10);
  }
}

void process_fft(fftwf_complex * fft_data, 
                 uint32_t size, 
                 uint32_t center_frequency,
                 uint32_t sample_rate)
{
  uint32_t start_frequency = center_frequency - sample_rate/2;
  uint32_t bin_step = sample_rate/size;
  float magnitudes[size];

  complex_to_magnitude(fft_data, size, magnitudes);

  for (uint32_t i = 0; i < size; i++) {
    uint32_t j = (i + size/2) % size;
    if (j == 0) continue;
    if (magnitudes[j] > threshold) {
      uint32_t frequency = start_frequency + i*bin_step;
      printf("freq %u power_db %f\n", frequency, magnitudes[j]);
    }
  }
}



