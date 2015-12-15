#pragma once

#include <vector>
#include <cstdint>

class SignalSource
{
 protected:
  bool m_doTiming;
  struct timespec m_start, m_stop;
  double m_elapsedTime;
  uint32_t m_retuneTimeIndex;
  uint32_t m_getSamplesTimeIndex;
  std::vector<double> m_retuneTime;
  std::vector<double> m_getSamplesTime;
  static const uint32_t s_maxIndex = 10000;

 protected:
  uint32_t m_sampleRate;
  uint32_t m_sampleCount;
  double m_startFrequency;
  double m_stopFrequency;
  std::vector<double> m_frequencies;
  uint32_t m_frequencyIndex;

 public:
  SignalSource(uint32_t m_sampleRate,
               uint32_t m_sampleCount,
               double m_startFrequency,
               double m_stopFrequency,
               bool doTiming = false);
  virtual ~SignalSource();
  virtual bool Start();
  virtual bool GetNextSamples(int16_t sample_buffer[][2], double_t & centerFrequency) = 0;
  virtual bool Stop();
  double GetNextFrequency();
  uint32_t GetFrequencyCount();
  void StartTimer();
  void StopTimer();
  void AddRetuneTime();
  void AddGetSamplesTime();
  void WriteTimingData();
};
