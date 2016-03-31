#include <math.h>
#include <time.h>
#include <stdio.h>
#include "fft.h"
#include "messageQueue.h"
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
    m_iterationLimit(0),
    m_thread(nullptr),
    m_finished(false),
    m_frequencyTable(sampleRate, startFrequency, stopFrequency),
    m_doTiming(doTiming),
    m_retuneTimeIndex(0),
    m_getSamplesTimeIndex(0),
    m_retuneTime(s_maxIndex),
    m_getSamplesTime(s_maxIndex)
{
}

SignalSource::~SignalSource() 
{
} 

bool SignalSource::Start() {}

bool SignalSource::Stop() {}

double SignalSource::GetNextFrequency(void ** pinfo)
{
  return this->m_frequencyTable.GetNextFrequency(pinfo);
}

double SignalSource::GetCurrentFrequency(void ** pinfo)
{
  return this->m_frequencyTable.GetCurrentFrequency(pinfo);
}

uint32_t SignalSource::GetFrequencyCount()
{
  return this->m_frequencyTable.GetFrequencyCount();
}

bool SignalSource::GetIsScanStart()
{
  return this->m_frequencyTable.GetIsScanStart();
}

uint32_t SignalSource::GetIterationCount()
{
  return this->m_frequencyTable.GetIterationCount();
}

bool SignalSource::StartThread(uint32_t numIterations, SampleQueue & sampleQueue)
{
  // NOTE: d_finished should be something explicitely thread safe. But since
  // nothing breaks on concurrent access, I'll just leave it as bool.
  printf("Starting source thread...\n");
  this->m_iterationLimit = numIterations;
  this->m_sampleQueue = &sampleQueue;
  this->m_finished = false;
  this->m_thread = std::unique_ptr<std::thread>(new std::thread(&SignalSource::ThreadWorkerHelper, 
                                                                this));
  return true;
}

bool SignalSource::StopThread()
{
  if (this->m_thread != nullptr) {
    printf("Stopping source thread...\n");
    // Shut down the thread
    this->m_finished = true;
    this->m_thread->join();
  }
  return true;
}

bool SignalSource::GetIsDone()
{
  if (this->GetIterationCount() >= this->m_iterationLimit || this->m_isDone) {
    return true;
  }
  return false;
}

void SignalSource::SetIsDone()
{
  this->m_isDone = true;
}

void SignalSource::ThreadWorkerHelper()
{
  this->ThreadWorker();
  this->m_sampleQueue->SetIsDone();
  this->m_sampleQueue = nullptr;
}

void SignalSource::StopStreaming()
{
  this->StopThread();
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

