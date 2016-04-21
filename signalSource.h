#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include <thread>
#include "frequencyTable.h"

class SignalSource
{
 protected:
  bool m_doTiming;
  struct timespec m_start, m_stop;
  double m_elapsedTime;
  uint32_t m_retuneTimeIndex;
  uint32_t m_getSamplesTimeIndex;
  bool m_isDone; // Set to true to terminate
  bool m_finished;
  std::unique_ptr<std::thread> m_thread;
  std::vector<double> m_retuneTime;
  std::vector<double> m_getSamplesTime;
  static const uint32_t s_maxIndex = 10000;

 protected:
  uint32_t m_sampleRate;
  uint32_t m_sampleCount;
  double m_startFrequency;
  double m_stopFrequency;
  FrequencyTable m_frequencyTable;
  uint32_t m_iterationLimit;
  SampleQueue * m_sampleQueue;
  void SetIsDone();
  bool StopThread();
  bool StartThread(uint32_t numIterations, SampleQueue & sampleQueue);
  void ThreadWorkerHelper();
  uint32_t GetIterationCount();
  double GetCurrentFrequency(void ** pinfo = nullptr);
  double GetNextFrequency(void ** pinfo = nullptr);
  bool GetIsDone();

 public:
  SignalSource(uint32_t m_sampleRate,
               uint32_t m_sampleCount,
               double m_startFrequency,
               double m_stopFrequency,
               bool doTiming = false);
  virtual ~SignalSource();
  virtual bool Start();
  virtual bool GetNextSamples(SampleQueue * sample_queue, double_t & centerFrequency) = 0;
  virtual bool StartStreaming(uint32_t numIterations, SampleQueue & sampleQueue) = 0;
  virtual void ThreadWorker() = 0;
  virtual bool Stop();
  virtual double Retune(double frequency) = 0;
  uint32_t GetFrequencyCount();
  bool GetIsScanStart();
  void StopStreaming();
  void StartTimer();
  void StopTimer();
  void AddRetuneTime();
  void AddGetSamplesTime();
  void WriteTimingData();
};
