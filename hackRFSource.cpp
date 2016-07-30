
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <iostream>
#include <limits>
#include <stdarg.h>
#include <cassert>
#include <vector>
#include <algorithm>
#include <sys/time.h>
#include "messageQueue.h"
#include "signalSource.h"
#include "hackRFSource.h"

#define HANDLE_ERROR(format, ...) this->handle_error(status, format, ##__VA_ARGS__)

void HackRFSource::handle_error(int status, const char * format, ...)
{
  if (status != 0) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    fprintf(stderr, buffer, hackrf_error_name(static_cast<hackrf_error>(status)));
    hackrf_close(this->m_dev);
    exit(1);
  }
}

HackRFSource::HackRFSource(std::string args,
                           uint32_t sampleRate, 
                           uint32_t sampleCount, 
                           double startFrequency, 
                           double stopFrequency)
  : SignalSource(sampleRate, sampleCount, startFrequency, stopFrequency, 0.75, 0.0),
    m_dev(nullptr),
    m_streamingState(Illegal),
    m_nextValidStreamTime{0, 0},
    m_retuneTime(0.0100),
    m_dropPacketCount(0), // ceil(sampleRate * m_retuneTime / 131072)),
    m_scanStartCount(101),
    m_centerFrequency(1e12),
    m_isScanStart(true),
    m_didRetune(false)
{
  int status;

  status = hackrf_init();
  HANDLE_ERROR("hackrf_init() failed: %%s\n");

  status = hackrf_open( &this->m_dev );
  HANDLE_ERROR("Failed to open HackRF device: %%s\n");

  uint8_t board_id;
  status = hackrf_board_id_read( this->m_dev, &board_id );
  HANDLE_ERROR("Failed to get HackRF board id: %%s\n");

  char version[128];
  memset(version, 0, sizeof(version));
  status = hackrf_version_string_read( this->m_dev, version, sizeof(version));
  HANDLE_ERROR("Failed to read version string: %%s\n");

  this->set_sample_rate(sampleRate);

  uint32_t bandWidth = hackrf_compute_baseband_filter_bw(uint32_t(0.75 * sampleRate));
  status = hackrf_set_baseband_filter_bandwidth( this->m_dev, bandWidth );
  HANDLE_ERROR("hackrf_set_baseband_filter_bandwidth %u: %%s", bandWidth );

  /* range 0-40 step 8d, IF gain in osmosdr  */
  hackrf_set_lna_gain(this->m_dev, 24);

  /* range 0-62 step 2db, BB gain in osmosdr */
  hackrf_set_vga_gain(this->m_dev, 28);

  /* Disable AMP gain stage by default. */
  hackrf_set_amp_enable(this->m_dev, 0);

  status = hackrf_set_antenna_enable(this->m_dev, 0);

  if (args.find("bias")) {
    /* antenna port power control */
    status = hackrf_set_antenna_enable(this->m_dev, 1);
    HANDLE_ERROR("Failed to enable antenna DC bias: %%s\n");
  }

  double centerFrequency = this->GetCurrentFrequency();
  this->Retune(centerFrequency);
}

HackRFSource::~HackRFSource()
{
  if (this->m_dev != nullptr) {
    int status = hackrf_stop_rx(this->m_dev);
    double centerFrequency = this->GetCurrentFrequency();
    HANDLE_ERROR("Failed to stop RX streaming at %u: %%s\n", centerFrequency);
    status = hackrf_close(this->m_dev);
    HANDLE_ERROR("Error closing hackrf: %%s\n");
  }
}

bool HackRFSource::Start()
{
  if (this->m_streamingState != Streaming) {
    this->m_streamingState = Streaming;
    int status = hackrf_start_rx(this->m_dev, 
                                 _hackRF_rx_callback, 
                                 (void *)this);
    HANDLE_ERROR("Failed to start RX streaming: %%s\n");
  }
  return true;
}

double HackRFSource::set_sample_rate( double rate )
{
  assert(this->m_dev != nullptr);

  int status = HACKRF_SUCCESS;
  double _sample_rates[] = {
    8e6,
    10e6,
    12.5e6,
    16e6,
    20e6};

  bool found_supported_rate = false;
  for( unsigned int i = 0; i < sizeof(_sample_rates)/sizeof(double); i++ ) {
    if(_sample_rates[i] == rate) {
      found_supported_rate = true;
      break;
    }
  }

  if (!found_supported_rate) {
    status = HACKRF_ERROR_OTHER;
    HANDLE_ERROR("Unsupported samplerate: %gMsps", rate/1e6);
  }

  status = hackrf_set_sample_rate( this->m_dev, rate);
  HANDLE_ERROR("Error setting sample rate to %gMsps: %%s\n", rate/1e6);
}

int HackRFSource::_hackRF_rx_callback(hackrf_transfer* transfer) 
{
  HackRFSource * obj = (HackRFSource *)transfer->rx_ctx;
  return obj->hackRF_rx_callback(transfer);
}

double HackRFSource::interpolateSamples(hackrf_transfer* transfer)
{
  uint32_t count = transfer->valid_length/2;
  uint16_t frequencyMhz;
  for (uint32_t i = 0; i < count; i += 8192) {
    int8_t post[2] = { (int8_t)transfer->buffer[2*(i+1)], (int8_t)transfer->buffer[2*(i+1)+1] };
    if (i > 0) {
      post[0] = (post[0] + (int8_t)transfer->buffer[2*(i-1)])/2;
      post[1] = (post[1] + (int8_t)transfer->buffer[2*(i-1)+1])/2;
    } else {
      frequencyMhz = *(uint16_t *)&transfer->buffer[0];
    }
    transfer->buffer[2*i] = post[0];
    transfer->buffer[2*i+1] = post[1];
  }
  // printf("interpolateSamples: frequency[%f]\n", double(frequencyMhz) * 1e5);
  return double(frequencyMhz) * 1e5;
}

int HackRFSource::hackRF_rx_callback(hackrf_transfer* transfer)
{
  if (this->m_streamingState != Streaming) {
    return 0;
  }
  // printf("hackRF_rx_callback valid_length:%d\n", transfer->valid_length);
  if (!this->GetIsDone()) { 
    double centerFrequency = this->interpolateSamples(transfer);
    if (centerFrequency != this->m_centerFrequency) {
      if (centerFrequency < this->m_centerFrequency) {
        this->m_isScanStart = true;
      }
      this->m_centerFrequency = centerFrequency;
      this->m_dropPacketCount = 2;
      return 0;
    }
    if (m_dropPacketCount > 0) {
      this->m_dropPacketCount--;
      return 0;
    }

    uint32_t count = transfer->valid_length/2;
    bool doRetune = false;
    time_t startTime = 0;
    if (this->m_isScanStart) {
      startTime = time(NULL);
    }

    assert(count >= this->m_sampleCount);

    if (this->GetFrequencyCount() > 1) {
      doRetune = true;
    }
    for (uint32_t i = 0; i < count; i+= this->m_sampleCount) {
      //printf("hackRF_rx_callback appending[%d] frequency[%f]\n", i, centerFrequency);
      this->m_sampleQueue->AppendSamples(reinterpret_cast<int8_t (*)[2]>(&transfer->buffer[2*i]),
                                         centerFrequency,
                                         startTime);
      this->m_isScanStart = false;
    }
#if 0
    if (doRetune) {
      StreamingState expected = Streaming;
      while (!this->m_streamingState.compare_exchange_strong(expected, DoRetune)) {
      }
    }
#endif
  } else {
    StreamingState expected = Streaming;
    while (!this->m_streamingState.compare_exchange_strong(expected, Done)) {
      if (expected == Done) {
        break;
      }
    }
  }
  
  return 0; // TODO: return -1 on error/stop
}

bool HackRFSource::GetNextSamples(SampleQueue * sampleQueue, double & centerFrequency)
{
  int status;
  uint32_t delta = 100;
  centerFrequency = this->GetNextFrequency();
  // sleep(0.010);
  //fprintf(stderr, "Tuned to %u\n", frequencies[j]);
  /* ... Handle signals at current frequency ... */
  this->m_streamingState = Streaming;

  while (this->m_streamingState != Done) {
  }

  if (this->GetFrequencyCount() > 1) {
    this->Retune(this->GetNextFrequency());
  }
  return true;
}

bool HackRFSource::StartStreaming(uint32_t numIterations, SampleQueue & sampleQueue)
{
  this->Start();
  auto result = this->StartThread(numIterations, sampleQueue);
  return result;
}

void HackRFSource::ThreadWorker()
{
  while (true) {
    StreamingState state = this->m_streamingState;
    switch (state) {
    case Streaming:
    case Done:
      break;
    case DoRetune:
      {
        double nextFrequency = this->GetNextFrequency();
        this->Retune(nextFrequency);
        this->m_didRetune = true;
        //struct timeval increment = {0, 5000}, currentTime;
        //gettimeofday(&currentTime, nullptr);
        //timeradd(&currentTime, &increment, &this->m_nextValidStreamTime);
        this->m_dropPacketCount = ceil(this->m_sampleRate * this->m_retuneTime / 131072);
        StreamingState expected = DoRetune;
        while (!this->m_streamingState.compare_exchange_strong(expected, Streaming)) {
        }
      }
      break;
    }
    if (state == Done) {
      break;
    }
  }
  int status = hackrf_stop_rx(this->m_dev);
  HANDLE_ERROR("Failed to stop RX streaming: %%s\n");
}

double HackRFSource::Retune(double centerFrequency)
{
  int status;
  // printf("Retuning to %.0f\n", centerFrequency);
  status = hackrf_set_freq(this->m_dev, uint64_t(centerFrequency));
  HANDLE_ERROR("Failed to tune to %.0f Hz: %%s\n", 
               centerFrequency);
  // printf("Retuned to %.0f\n", centerFrequency);
  return centerFrequency;
}
