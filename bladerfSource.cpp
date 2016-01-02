#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <iostream>
#include <limits>
#include <stdarg.h>
#include <libbladeRF.h>
#include "messageQueue.h"
#include "signalSource.h"
#include "bladerfSource.h"

#define HANDLE_ERROR(format, ...) this->handle_error(this->m_dev, status, format, ##__VA_ARGS__)

BladerfSource::~BladerfSource()
{
  bladerf_close(this->m_dev);
}

int BladerfSource::configure_module(struct bladerf *dev, struct module_config *c)
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

bool BladerfSource::populate_quick_tunes()
{
  int status;
  unsigned int i, j;
  /* Get our quick tune parameters for each frequency we'll be using */
  for (i = 0; i < this->m_frequencies.size(); i++) {
    status = bladerf_set_frequency(this->m_dev, BLADERF_MODULE_RX, this->m_frequencies[i]);
    HANDLE_ERROR("Failed to set frequency to %u Hz: %%s\n", this->m_frequencies[i]);
    status = bladerf_get_quick_tune(this->m_dev, BLADERF_MODULE_RX, &this->m_quickTunes[i]);
    HANDLE_ERROR("Failed to get quick tune for %u Hz: %%s\n", this->m_frequencies[i]);
  }
  return true;
}

void BladerfSource::handle_error(struct bladerf * dev, int status, const char * format, ...)
{
  if (status != 0) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    fprintf(stderr, buffer, bladerf_strerror(status));
    bladerf_close(dev);
    exit(1);
  }
}

BladerfSource::BladerfSource(std::string args,
                             uint32_t sampleRate, 
                             uint32_t sampleCount, 
                             double startFrequency, 
                             double stopFrequency)
  : SignalSource(sampleRate, sampleCount, startFrequency, stopFrequency)
{
  int status;
  struct module_config config;
  struct bladerf_devinfo dev_info;

  /* Initialize the information used to identify the desired device
   * to all wildcard (i.e., "any device") values */
  bladerf_init_devinfo(&dev_info);

  /* Request a device with the provided serial number.
   * Invalid strings should simply fail to match a device. */
  // if (argc >= 2) {
  //  strncpy(dev_info.serial, argv[1], sizeof(dev_info.serial) - 1);
  //}
  status = bladerf_open_with_devinfo(&this->m_dev, &dev_info);
  HANDLE_ERROR("Unable to open device: %%s\n");

  /* Set up RX module parameters */
  config.module = BLADERF_MODULE_RX;
  config.frequency = 619000000;
  config.bandwidth = sampleRate;
  config.samplerate = sampleRate;
  config.rx_lna = BLADERF_LNA_GAIN_MAX;
  config.vga1 = 30;
  config.vga2 = 3;
  status = configure_module(this->m_dev, &config);
  HANDLE_ERROR("Failed to configure RX module. Exiting.\n");

  /* Set up TX module parameters */
  config.module = BLADERF_MODULE_TX;
  config.frequency = 918000000;
  config.bandwidth = 1500000;
  config.samplerate = 250000;
  config.vga1 = -14;
  config.vga2 = 0;
  status = configure_module(this->m_dev, &config);
  HANDLE_ERROR("Failed to configure TX module. Exiting.\n");

  /* Application code goes here.
   *
   * Don't forget to call bladerf_enable_module() before attempting to
   * transmit or receive samples!
   */
  
  bladerf_enable_module(this->m_dev, BLADERF_MODULE_RX, true);
  bladerf_enable_module(this->m_dev, BLADERF_MODULE_TX, false);

  const uint32_t num_samples = 8192;
  status = bladerf_sync_config(this->m_dev,
                               BLADERF_MODULE_RX,
                               BLADERF_FORMAT_SC16_Q11_META,
                               8,
                               num_samples,
                               4,
                               0);
  
  bladerf_enable_module(this->m_dev, BLADERF_MODULE_RX, true);
  this->m_quickTunes = new struct bladerf_quick_tune[this->m_frequencies.size()];
  this->populate_quick_tunes();
  this->Retune(this->GetCurrentFrequency());
}

bool BladerfSource::GetNextSamples(SampleQueue * sampleQueue, double & centerFrequency)
{
  int status;
  uint32_t delta = 100;
  int16_t sample_buffer[this->m_sampleCount][2];

  /* Retrieve the current timestamp */
  struct bladerf_metadata metadata;
  memset(&metadata, 0, sizeof(metadata));
  status = bladerf_get_timestamp(this->m_dev, 
                                 BLADERF_MODULE_RX, 
                                 &metadata.timestamp);
  HANDLE_ERROR("Failed to get current RX timestamp: %s\n");
  fprintf(stderr, "Current RX timestamp: 0x%016lx ", metadata.timestamp);

  /* Schedule the RX to be ~1 ms in the future */
  struct bladerf_metadata metadata2;
  memset(&metadata2, 0, sizeof(metadata2));
  // metadata2.timestamp += this->m_sampleRate;
  // fprintf(stderr, " 0x%016lx ", metadata2.timestamp);
  metadata2.flags = BLADERF_META_FLAG_RX_NOW;

  // sleep(0.010);
  //fprintf(stderr, "Tuned to %u\n", frequencies[j]);
  /* ... Handle signals at current frequency ... */
  while (true) {
    status = bladerf_sync_rx(this->m_dev,
                             sample_buffer,
                             this->m_sampleCount,
                             &metadata2,
                             0);
                             
    fprintf(stderr, " 0x%016lx\n", metadata2.timestamp);
    HANDLE_ERROR("Failed to receive samples at %u Hz: %%s\n", 
                 this->m_frequencies[this->m_frequencyIndex]);
    if (metadata2.timestamp >= metadata.timestamp) {
      break;
    }
  }
  if (this->GetFrequencyCount() > 1) {
    this->Retune(this->GetNextFrequency());
  }
  this->m_sampleQueue->AppendSamples(sample_buffer, centerFrequency);
  return true;
}

bool BladerfSource::StartStreaming(uint32_t numIterations, SampleQueue & sampleQueue)
{
  this->m_iterationCount = numIterations;
  this->m_sampleQueue = &sampleQueue;
  auto result = this->StartThread();
  return result;
}

void BladerfSource::ThreadWorker()
{
  int status;
  uint32_t delta = 100;
  int16_t sample_buffer[this->m_sampleCount][2];

  while (this->GetIterationCount() > 0) {
    double centerFrequency = this->GetCurrentFrequency();
    /* Retrieve the current timestamp */
    struct bladerf_metadata metadata;
    memset(&metadata, 0, sizeof(metadata));
    status = bladerf_get_timestamp(this->m_dev, 
                                   BLADERF_MODULE_RX, 
                                   &metadata.timestamp);
    HANDLE_ERROR("Failed to get current RX timestamp: %s\n");
    //fprintf(stderr, "Current RX timestamp: 0x%016lx ", metadata.timestamp);

    /* Schedule the RX to be ~1 ms in the future */
    struct bladerf_metadata metadata2;
    memset(&metadata2, 0, sizeof(metadata2));
    // metadata2.timestamp += this->m_sampleRate;
    // fprintf(stderr, " 0x%016lx ", metadata2.timestamp);
    metadata2.flags = BLADERF_META_FLAG_RX_NOW;

    // sleep(0.010);
    //fprintf(stderr, "Tuned to %u\n", frequencies[j]);
    /* ... Handle signals at current frequency ... */
    while (true) {
      status = bladerf_sync_rx(this->m_dev,
                               sample_buffer,
                               this->m_sampleCount,
                               &metadata2,
                               0);
                             
      //fprintf(stderr, " 0x%016lx\n", metadata2.timestamp);
      HANDLE_ERROR("Failed to receive samples at %u Hz: %%s\n", 
                   this->m_frequencies[this->m_frequencyIndex]);
      if (metadata2.timestamp >= metadata.timestamp) {
        break;
      }
    }
    double nextFrequency = this->GetNextFrequency();
    if (this->GetFrequencyCount() > 1) {
      this->Retune(nextFrequency);
    }
    this->m_sampleQueue->AppendSamples(sample_buffer, centerFrequency);
  }
  return;
}

double BladerfSource::Retune(double frequency)
{
  int status;
  double centerFrequency = this->GetCurrentFrequency();
  /* Tune to the specified frequency immediately via BLADERF_RETUNE_NOW.
   *
   * Alternatively, this re-tune could be scheduled by providing a
   * timestamp counter value */
  status = bladerf_schedule_retune(this->m_dev, 
                                   BLADERF_MODULE_RX, 
                                   BLADERF_RETUNE_NOW,
                                   0,
                                   &this->m_quickTunes[this->m_frequencyIndex]);
  HANDLE_ERROR("Failed to apply quick tune at %.0f Hz: %%s\n", 
               centerFrequency);
  return centerFrequency;
}
