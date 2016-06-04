#include <vector>
#include <stdio.h>
#include <cstdint>
#include <math.h>
#include <cassert>
#include "frequencyTable.h"


FrequencyTable::FrequencyTable(uint32_t sampleRate,
                               double startFrequency,
                               double stopFrequency)
  : m_frequencyIndex(0),
    m_iterationCount(0)
{
  double f1 = startFrequency + 0.375 * sampleRate;
  double step = 0.375 - 0.025;
  double frequency;
  uint32_t count = 0;
  for (; (frequency = f1 + count * step * double(sampleRate)) < stopFrequency; count++) {
  }
  assert(count == ceil((stopFrequency - f1)/(step * sampleRate)));
  this->m_table.resize(count);
  for (uint32_t i = 0; i < count; i++) {
    double frequency = f1 + i * 0.40 * double(sampleRate);
    // double frequency = startFrequency + i * double(sampleRate) + double(sampleRate)/2;
    printf("Frequency %d: %.0f\n", i, frequency);
    this->m_table.at(i) = FrequencyInfo{frequency, nullptr};
  }
}

double FrequencyTable::GetNextFrequency(void ** pinfo)
{
  this->m_frequencyIndex++;
  if (this->m_frequencyIndex >= this->m_table.size()) {
    this->m_frequencyIndex = 0;
    this->m_iterationCount++;
  }
  return this->GetCurrentFrequency(pinfo);
}

double FrequencyTable::GetCurrentFrequency(void ** pinfo)
{
  FrequencyInfo & finfo = this->m_table[this->m_frequencyIndex];
  if (pinfo != nullptr) {
    *pinfo = finfo.m_info;
  }
  return finfo.m_frequency;
}

uint32_t FrequencyTable::GetFrequencyCount()
{
  return this->m_table.size();
}

double FrequencyTable::GetFrequencyFromIndex(uint32_t index)
{
  assert(index < this->m_table.size());
  FrequencyInfo & finfo = this->m_table[index];
  return finfo.m_frequency;
}

void FrequencyTable::SetFrequencyInfoForIndex(uint32_t index, void * info)
{
  assert(index < this->m_table.size());
  FrequencyInfo & finfo = this->m_table[index];
  finfo.m_info = info;
}

uint32_t FrequencyTable::GetIterationCount()
{
  return this->m_iterationCount; 
}

bool FrequencyTable::GetIsScanStart()
{
  return this->m_frequencyIndex == 0;
}
