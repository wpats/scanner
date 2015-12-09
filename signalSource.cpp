#include <math.h>
#include <time.h>
#include <stdio.h>
#include "signalSource.h"

SignalSource::SignalSource(uint32_t sampleRate,
                           uint32_t sampleCount,
                           double startFrequency,
                           double stopFrequency,
                           bool doTiming) 
  : m_sampleRate(sampleRate),
    m_sampleCount(sampleCount),
    m_startFrequency(startFrequency),
    m_stopFrequency(stopFrequency),
    m_frequencyIndex(0),
    m_doTiming(doTiming),
    m_retuneTimeIndex(0),
    m_getSamplesTimeIndex(0),
    m_retuneTime(s_maxIndex),
    m_getSamplesTime(s_maxIndex)
{
  uint32_t numFrequencies = ceil((stopFrequency - startFrequency)/sampleRate);
  this->m_frequencies.resize(numFrequencies);
  for (uint32_t i = 0; i < numFrequencies; i++) {
    this->m_frequencies[i] = startFrequency + i * sampleRate + sampleRate/2;
    fprintf(stderr, "Frequency %d: %f\n", i, this->m_frequencies[i]);
  }
}

SignalSource::~SignalSource() 
{
} 

bool SignalSource::Start() {}

bool SignalSource::Stop() {}

double SignalSource::GetNextFrequency()
{
  uint32_t index = this->m_frequencyIndex;
  this->m_frequencyIndex = (this->m_frequencyIndex + 1) % this->m_frequencies.size();
  return this->m_frequencies[index];
}

uint32_t SignalSource::GetFrequencyCount()
{
  return this->m_frequencies.size();
}

void SignalSource::StartTimer()
{
  if (this->m_doTiming) {
    clock_gettime(CLOCK_REALTIME, &this->m_start);
  }
}
void SignalSource::StopTimer()
{
  if (this->m_doTiming) {
    clock_gettime(CLOCK_REALTIME, &this->m_stop);
    double startd = this->m_start.tv_sec*1000.0 + this->m_start.tv_nsec/1e6;
    double stopd = this->m_stop.tv_sec*1000.0 + this->m_stop.tv_nsec/1e6;
    this->m_elapsedTime = stopd - startd;
  }
}
void SignalSource::AddRetuneTime()
{
  if (this->m_doTiming && this->m_retuneTimeIndex < s_maxIndex) {
    this->m_retuneTime[this->m_retuneTimeIndex++] = this->m_elapsedTime;
  }
}
void SignalSource::AddGetSamplesTime()
{
  if (this->m_doTiming && this->m_getSamplesTimeIndex < s_maxIndex) {
    this->m_getSamplesTime[this->m_getSamplesTimeIndex++] = this->m_elapsedTime;
  }
}
void SignalSource::WriteTimingData()
{
  if (this->m_doTiming && this->m_retuneTimeIndex >= s_maxIndex) {
    FILE * outFile = fopen("timings.txt", "w");
    if (outFile != nullptr) {
      for (uint32_t i = 0; i < this->m_retuneTimeIndex; i++) {
        fprintf(outFile, "%f, %f\n", this->m_retuneTime[i], this->m_getSamplesTime[i]);
      }
      fclose(outFile);
    }
    this->m_doTiming = false;
  }
}
