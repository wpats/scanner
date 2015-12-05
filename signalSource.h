#pragma once

#include <vector>

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
 public:
  SignalSource(bool doTiming = false) 
    : m_doTiming(doTiming),
      m_retuneTimeIndex(0),
      m_getSamplesTimeIndex(0),
      m_retuneTime(s_maxIndex),
      m_getSamplesTime(s_maxIndex)
  {}
  virtual ~SignalSource() 
  {
  } 
  virtual bool Start() {}
  virtual bool GetNextSamples(int16_t sample_buffer[][2], double_t & centerFrequency) = 0;
  virtual bool Stop() {}
  void StartTimer()
  {
    if (this->m_doTiming) {
      clock_gettime(CLOCK_REALTIME, &this->m_start);
    }
  }
  void StopTimer()
  {
    if (this->m_doTiming) {
      clock_gettime(CLOCK_REALTIME, &this->m_stop);
      double startd = this->m_start.tv_sec*1000.0 + this->m_start.tv_nsec/1e6;
      double stopd = this->m_stop.tv_sec*1000.0 + this->m_stop.tv_nsec/1e6;
      this->m_elapsedTime = stopd - startd;
    }
  }
  void AddRetuneTime()
  {
    if (this->m_doTiming && this->m_retuneTimeIndex < s_maxIndex) {
      this->m_retuneTime[this->m_retuneTimeIndex++] = this->m_elapsedTime;
    }
  }
  void AddGetSamplesTime()
  {
    if (this->m_doTiming && this->m_getSamplesTimeIndex < s_maxIndex) {
      this->m_getSamplesTime[this->m_getSamplesTimeIndex++] = this->m_elapsedTime;
    }
  }
  void WriteTimingData()
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
};
