
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <iostream>
#include <limits>
#include <stdarg.h>
#include <cassert>
#include <mirsdrapi-rsp.h>
#include "signalSource.h"
#include "sdrplaySource.h"

#define HANDLE_ERROR(format, ...) this->handle_error(status, format, ##__VA_ARGS__)

const char * SdrplaySource::errorToString(mir_sdr_ErrT code)
{
  static const char * s_errorStrings[] = {
    "Success",
    "Fail",
    "Invalid Parameter",
    "Out of range",
    "Gain Update Error",
    "Rf Update Error",
    "Fs Update Error",
    "Hardware Error",
    "Aliasing Error",
    "Already Initialised",
    "Not Initialised"
  };
  assert(code <= mir_sdr_NotInitialised);
  return s_errorStrings[code];
}

void SdrplaySource::handle_error(mir_sdr_ErrT status, const char * format, ...)
{
  if (status != mir_sdr_Success) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    fprintf(stderr, buffer, this->errorToString(status));
    mir_sdr_Uninit();
    exit(1);
  }
}

SdrplaySource::SdrplaySource(std::string args,
                           uint32_t sampleRate, 
                           uint32_t sampleCount, 
                           double startFrequency, 
                           double stopFrequency)
  : SignalSource(sampleRate, sampleCount, startFrequency, stopFrequency),
    m_samplesPerPacket(0),
    m_firstSampleNum(0),
    m_sample_buffer_i(nullptr),
    m_sample_buffer_q(nullptr)
{
  mir_sdr_ErrT status;
  int gRdB = 60;
  // Check API version
  float version;
  status = mir_sdr_ApiVersion(&version);
  if (version != MIR_SDR_API_VERSION) {
    // include file does not match dll. Deal with error condition.
    fprintf(stderr, "API version does not match dll\n");
    exit(1);
  }

  double centerFrequency = this->m_frequencies[this->m_frequencyIndex]/1e6;
  // Initialise API and hardware  
  status = mir_sdr_Init(gRdB, 
                        double(sampleRate/1e6),
                        centerFrequency,
                        mir_sdr_BW_8_000,
                        mir_sdr_IF_Zero,
                        &this->m_samplesPerPacket);
  HANDLE_ERROR("Failed to initialize Sdrplay device: %%s\n");


  uint32_t bufferSize = this->m_samplesPerPacket * (ceil(double(sampleCount)/this->m_samplesPerPacket));
 
  this->m_sample_buffer_i = new int16_t[bufferSize];
  this->m_sample_buffer_q = new int16_t[bufferSize];

  // Configure DC tracking in tuner
  status = mir_sdr_SetDcMode(0, 0);
  // status = mir_sdr_SetDCTrackTime(63);
}

SdrplaySource::~SdrplaySource()
{
  mir_sdr_ErrT status = mir_sdr_Uninit();
  HANDLE_ERROR("Failed to uninitialize Sdrplay device: %%s\n");
}

bool SdrplaySource::GetNextSamples(int16_t sample_buffer[][2], double & centerFrequency)
{
  mir_sdr_ErrT status;

  centerFrequency = this->GetNextFrequency();
  if (this->GetFrequencyCount() > 1) {
    status = mir_sdr_SetRf(centerFrequency, 1, 0);
    HANDLE_ERROR("Failed to tune  %f Hz: %%s\n", 
                 centerFrequency);
  }

  /* ... Handle signals at current frequency ... */
  for (uint32_t count = 0; 
       count < this->m_sampleCount; 
       count += this->m_samplesPerPacket) {
    int grChanged = 0;
    int rfChanged = 0;
    int fsChanged = 0;
    status =  mir_sdr_ReadPacket(&this->m_sample_buffer_i[count],
                                 &this->m_sample_buffer_q[count],
                                 &this->m_firstSampleNum,
                                 &grChanged,
                                 &rfChanged,
                                 &fsChanged);
    HANDLE_ERROR("Error receiving samples at %f[%u] : %%s\n", 
                 centerFrequency,
                 count);
  }
  for (uint32_t i = 0; i < this->m_sampleCount; i++) {
    sample_buffer[i][0] = this->m_sample_buffer_i[i];
    sample_buffer[i][1] = this->m_sample_buffer_q[i];
  }
  return true;
}
