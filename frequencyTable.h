#pragma once

class FrequencyTable
{
  struct FrequencyInfo
  {
    double m_frequency;
    void * m_info;
  };
  std::vector<FrequencyInfo> m_table;
  uint32_t m_frequencyIndex;
  uint32_t m_iterationCount;

 public:
  FrequencyTable(uint32_t m_sampleRate,
                 double m_startFrequency,
                 double m_stopFrequency,
                 double useBandWidth,
                 double dcIgnoreWidth);
  double GetNextFrequency(void ** pinfo = nullptr);
  double GetCurrentFrequency(void ** pinfo = nullptr);
  uint32_t GetFrequencyCount();
  double GetFrequencyFromIndex(uint32_t index);
  void SetFrequencyInfoForIndex(uint32_t index, void * info);
  uint32_t GetIterationCount();
  bool GetIsScanStart();
};
