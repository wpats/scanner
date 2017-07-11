
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
#include <libairspy/airspy.h>
#include "messageQueue.h"
#include "signalSource.h"
#include "rtlSource.h"
#include "arguments.h"

#define HANDLE_ERROR(format, ...) this->handle_error(status, format, ##__VA_ARGS__)

void RtlSource::handle_error(int status, const char * format, ...)
{
  if (status != 0) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    fprintf(stderr, buffer, status);
    if (this->m_dev != nullptr) {
      rtlsdr_close(this->m_dev);
    }
    exit(1);
  }
}

RtlSource::RtlSource(std::string args,
                     uint32_t sampleRate, 
                     uint32_t sampleCount, 
                     double startFrequency, 
                     double stopFrequency)
  : SignalSource(sampleRate, sampleCount, startFrequency, stopFrequency),
    m_dev(nullptr),
    m_streamingState(Illegal),
    m_bufferIndex(0),
    m_retuneTime(0.005),
    m_dropPacketValue(3), // What should this be ? ceil(sampleRate * m_retuneTime / 65536)
    m_dropPacketCount(m_dropPacketValue)
{
  int direct_samp = 0;
  uint32_t dev_index = 0;
  bool auto_gain = false;
  int status;

  Arguments arguments(args);

  this->m_buffer = new int8_t[sampleCount][2];
  assert(this->m_buffer != nullptr);

  char manufact[256];
  char product[256];
  char serial[256];
  memset(manufact, 0, sizeof(manufact));
  memset(product, 0, sizeof(product));
  memset(serial, 0, sizeof(serial));
  if ( !rtlsdr_get_device_usb_strings( dev_index, manufact, product, serial ) ) {
    if (strlen(manufact))
      std::cerr << " " << manufact;
    if (strlen(product))
      std::cerr << " " << product;
    if (strlen(serial))
      std::cerr << " SN: " << serial;
  } else {
    std::cerr << " " << rtlsdr_get_device_name(dev_index);
  }

  if (arguments.HasValue("rtl")) {
    dev_index = arguments.GetIntValue("rtl");
  }
  if (arguments.HasValue("direct_samp")) {
    direct_samp = arguments.GetIntValue("direct_samp");
  }

  status = rtlsdr_open( &this->m_dev, dev_index );
  HANDLE_ERROR("Failed to open rtl device %d: %%d\n", dev_index);

  status = rtlsdr_set_sample_rate( this->m_dev, sampleRate );
  HANDLE_ERROR("Failed to set samplerate %d: %%d\n", sampleRate);

  status = rtlsdr_set_tuner_gain_mode( this->m_dev, int(!auto_gain) );
  HANDLE_ERROR("Failed to set tuner gain mode: %%d\n");
 
  status = rtlsdr_set_agc_mode( this->m_dev, int(auto_gain) );
  HANDLE_ERROR("Failed to set agc mode: %%d\n");

  if (direct_samp) {
    status = rtlsdr_set_direct_sampling( this->m_dev, direct_samp );
    HANDLE_ERROR("Failed to enable direct sampling: %%d\n");
  }

  status = rtlsdr_set_tuner_gain( this->m_dev, 140 );
  HANDLE_ERROR("Failed to set tuner gain: %%d\n");

#if 0
  // This is only for E4000 tuners.
  status = rtlsdr_set_tuner_if_gain( this->m_dev, 70 );
  HANDLE_ERROR("Failed to set if gain: %%d\n");
#endif

  double centerFrequency = this->GetCurrentFrequency();
  this->Retune(centerFrequency);
}

RtlSource::~RtlSource()
{
  if (this->m_dev != nullptr) {
#if 0
    int status = rtlsdr_cancel_async(this->m_dev);
    double centerFrequency = this->GetCurrentFrequency();
    HANDLE_ERROR("Failed to stop RX streaming at %u: %%d\n", centerFrequency);
#endif
    int status = rtlsdr_close(this->m_dev);
    HANDLE_ERROR("Failed to close device: %%d\n");
  }
}

bool RtlSource::Start()
{
  if (this->m_streamingState != Streaming) {
#if 0
    int status = rtlsdr_read_async(this->m_dev, 
                                   _rtl_rx_callback, 
                                   (void *)this,
                                   0,
                                   0);
    HANDLE_ERROR("Failed to start RX streaming: %%d\n");
#endif
    this->m_streamingState = Streaming;
  }
  return true;
}

double RtlSource::set_sample_rate( double rate )
{
  assert(this->m_dev != nullptr);

  int status = rtlsdr_set_sample_rate( this->m_dev, uint32_t(rate) );
  HANDLE_ERROR("Failed to set samplerate %d: %%d\n", uint32_t(rate) );
}

void RtlSource::_rtl_rx_callback(unsigned char *buf, uint32_t len, void *ctx)
{
  RtlSource * obj = (RtlSource *)ctx;
  obj->rtl_rx_callback(buf, len/2);
}

int RtlSource::rtl_rx_callback(void * samples, int sample_count)
{
  time_t startTime;

  if (!this->GetIsDone()) { 
    if (this->m_dropPacketCount-- > 0) {
      return 0;
    }
    double centerFrequency = this->GetCurrentFrequency();
    bool isScanStart = this->GetIsScanStart();
    startTime = time(NULL);

    double nextFrequency = this->GetNextFrequency();
    if (this->GetFrequencyCount() > 1) {
      this->Retune(nextFrequency);
      this->m_dropPacketCount = this->m_dropPacketValue;
    }
    for (uint32_t i = 0; i < sample_count/this->m_sampleCount; i++) {
      this->m_sampleQueue->AppendSamples(reinterpret_cast<int8_t (*)[2]>(samples) + i*sample_count,
                                         centerFrequency,
                                         (isScanStart ? startTime : 0));
      isScanStart = false;
    }
  } else {
    this->m_streamingState = Done;
  }
  return 0;
}

bool RtlSource::StartStreaming(uint32_t numIterations, SampleQueue & sampleQueue)
{
  auto result = this->StartThread(numIterations, sampleQueue);
  this->Start();
  return result;
}

void RtlSource::ThreadWorker()
{
  while (this->m_streamingState != Done) {
    time_t startTime;
    if (!this->GetIsDone()) { 
      double centerFrequency = this->GetCurrentFrequency();
      bool isScanStart = this->GetIsScanStart();
      startTime = time(NULL);

      // Read samples synchronously.
      int n_read;
      int status;
      rtlsdr_reset_buffer(this->m_dev);
      HANDLE_ERROR("Failed to reset buffer: %%d\n");
      rtlsdr_read_sync(this->m_dev, 
                       this->m_buffer,
                       2 * this->m_sampleCount,
                       &n_read);
      HANDLE_ERROR("Failed to read samples: %%d\n");
      assert(n_read == 2 * this->m_sampleCount);

      double nextFrequency = this->GetNextFrequency();
      if (this->GetFrequencyCount() > 1) {
        this->Retune(nextFrequency);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        this->m_dropPacketCount = this->m_dropPacketValue;
      }

      this->m_sampleQueue->AppendSamples(this->m_buffer,
                                         centerFrequency,
                                         (isScanStart ? startTime : 0));
      isScanStart = false;
    } else {
      this->m_streamingState = Done;
    }
  }
}

double RtlSource::Retune(double centerFrequency)
{
  assert(this->m_dev != nullptr);
  int status = rtlsdr_set_center_freq(this->m_dev, uint32_t(centerFrequency));
  HANDLE_ERROR("Failed to tune to %d Hz: %%d\n", uint32_t(centerFrequency));
  return centerFrequency;
}

// Unimplemented.
bool RtlSource::GetNextSamples(SampleQueue * sampleQueue, double & centerFrequency)
{
  return false;
}
