#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <iostream>
#include <limits>
#include <cassert>
#include <volk/volk.h>
#include "fft.h"
#include "messageQueue.h"
#include "signalSource.h"
#include "process.h"

FFTWindow::FFTWindow(gr::fft::window::win_type type, uint32_t numSamples)
  : m_type(type),
    m_numSamples(numSamples)
{
  this->m_windowVector = gr::fft::window::build(type, numSamples, 0.0);
  this->m_window = reinterpret_cast<float *>(fftwf_malloc(sizeof(float) * numSamples));
  memcpy(this->m_window, &this->m_windowVector[0], sizeof(float) * numSamples);
}

FFTWindow::~FFTWindow()
{
  fftwf_free(this->m_window);
}

void FFTWindow::apply(fftwf_complex * samples)
{
  volk_32fc_32f_multiply_32fc_a(reinterpret_cast<lv_32fc_t *>(samples), 
                                reinterpret_cast<lv_32fc_t *>(samples), 
                                this->m_window, 
                                this->m_numSamples);
}

bool ProcessSamples::process_fft(fftwf_complex * fft_data, 
                                 uint32_t center_frequency)
{
  uint32_t start_frequency = center_frequency - this->m_sampleRate/2;
  uint32_t bin_step = this->m_sampleRate/this->m_sampleCount;
  float magnitudes[this->m_sampleCount];

  Utility::complex_to_magnitude(fft_data, magnitudes, this->m_sampleCount);
  bool trigger = false;
  for (uint32_t i = 0; i < this->m_sampleCount; i++) {
    uint32_t j = (i + this->m_sampleCount/2) % this->m_sampleCount;
    if (j < this->m_dcIgnoreWindow || (this->m_sampleCount - j < this->m_dcIgnoreWindow)) {
      continue;
    }
    if (magnitudes[j] > this->m_threshold) {
      uint32_t frequency = start_frequency + i*bin_step;
      printf("freq %u power_db %f\n", frequency, magnitudes[j]);
      trigger = true;
    }
  }
  return trigger;
}

ProcessSamples::ProcessSamples(uint32_t numSamples, 
                               uint32_t sampleRate, 
                               uint32_t enob,
                               float threshold,
                               gr::fft::window::win_type windowType,
                               bool correctDCOffset,
                               Mode mode,
                               std::string fileNameBase,
                               uint32_t dcIgnoreWindow,
                               uint32_t preTrigger,
                               uint32_t postTrigger)
  : m_sampleCount(numSamples),
    m_sampleRate(sampleRate),
    m_enob(enob),
    m_fileCounter(0),
    m_threshold(threshold),
    m_fftWindow(windowType, numSamples),
    m_correctDCOffset(correctDCOffset),
    m_dcIgnoreWindow(dcIgnoreWindow),
    m_fft(numSamples),
    m_mode(mode),
    m_sampleQueue(nullptr),
    m_fileNameBase(fileNameBase),
    m_preTrigger(preTrigger),
    m_postTrigger(postTrigger),
    m_writing(false),
    m_endSequenceId(0)
{
  assert(mode > Illegal && mode <= FrequencyDomain);
  m_inputSamples = reinterpret_cast<fftwf_complex *>(fftwf_alloc_complex(numSamples));
  m_fftOutputBuffer = reinterpret_cast<fftwf_complex *>(fftwf_alloc_complex(numSamples));
}

ProcessSamples::~ProcessSamples()
{
  fftwf_free(this->m_inputSamples);
  fftwf_free(this->m_fftOutputBuffer);
}

void ProcessSamples::WriteToFile(const char * fileName, fftwf_complex * data)
{
  FILE * outFile = fopen(fileName, "w");
  if (outFile == NULL) {
    fprintf(stderr, "Failed to open file '%s'\n", fileName);
    return;
  }
  for (uint32_t i = 0; i < this->m_sampleCount; i++) {
    fprintf(outFile, "%f, %f\n", double(data[i][0]), double(data[i][1]));
  }
  fclose(outFile);
}

void ProcessSamples::Run(int16_t sample_buffer[][2], uint32_t centerFrequency)
{
  Utility::short_complex_to_float_complex(sample_buffer, 
                                          this->m_inputSamples,
                                          this->m_sampleCount,
                                          this->m_enob,
                                          this->m_correctDCOffset);
  if (this->m_mode == FrequencyDomain) {
    this->m_fftWindow.apply(this->m_inputSamples);
    this->m_fft.process(this->m_fftOutputBuffer, this->m_inputSamples);
    this->process_fft(this->m_fftOutputBuffer, centerFrequency);
  }
}

std::string ProcessSamples::GenerateFileName(std::string fileNameBase, 
                                             time_t startTime, 
                                             double_t centerFrequency)
{
  char buffer[256];
  const char timeformat[] = "-%Y%m%d-%T";
  struct tm * timeStruct = localtime(&startTime);
  if (timeStruct == nullptr) {
    perror("localtime");
    exit(1);
  }
  strcpy(buffer, fileNameBase.c_str());
  uint32_t length = strlen(buffer);
  if (strftime(buffer+length, sizeof(buffer)-length, timeformat, timeStruct) == 0) {
    fprintf(stderr, "strftime returned 0");
    exit(1);
  }
  sprintf(buffer+strlen(buffer), "-%.0f-%u", centerFrequency, ++this->m_fileCounter);
  return std::string(buffer);
}

void ProcessSamples::WriteSamplesToFile(uint64_t sequenceId, double centerFrequency)
{
  assert(this->m_sampleQueue != nullptr);
  time_t startTime;
  startTime = time(NULL);
  std::string fileName = this->GenerateFileName(this->m_fileNameBase, startTime, centerFrequency);
  uint64_t decrement = std::min<uint64_t>(sequenceId, this->m_preTrigger);
  this->m_sampleQueue->BeginWrite(sequenceId - decrement, fileName);
}

void ProcessSamples::RecordSamples(SignalSource * signalSource, 
                                   uint64_t count,
                                   double threshold)
{
  int16_t sample_buffer[this->m_sampleCount][2];
  double_t centerFrequency;
  time_t startTime;
  for (uint64_t i = 0; i < count; i += this->m_sampleCount) {
    startTime = time(NULL);
    Utility::short_complex_to_float_complex(sample_buffer, 
                                            this->m_inputSamples,
                                            this->m_sampleCount,
                                            this->m_enob,
                                            this->m_correctDCOffset);
    // Check for threshold.
    std::string fileName = this->GenerateFileName(this->m_fileNameBase, startTime, centerFrequency);
    // this->WriteSamplesToFile(fileName, this->m_sampleCount);
   }
}

bool ProcessSamples::DoTimeDomainThresholding(fftwf_complex * inputSamples,
                                              double centerFrequency)
{
  double log10 = log2(10);
  float max = std::numeric_limits<float>::min();
  float min = std::numeric_limits<float>::max();
  bool allZeros = true;
  for (uint32_t i = 0; i < this->m_sampleCount; i++) {
    float re = inputSamples[i][0];
    float im = inputSamples[i][1];
    if (re != 0.0 || im != 0.0) {
      allZeros = false;
    }
    float mag = sqrt(re * re + im * im); // / this->m_sampleCount;
    float magnitude = 10 * log2(mag) / log10;
    max = std::max(max, magnitude);
    min = std::min(min, magnitude);
    if (magnitude > this->m_threshold) {
      printf("Signal %f above threshold %f at frequency %.0f", 
             magnitude, 
             this->m_threshold,
             centerFrequency);
      return true;
    }
  }  
  // printf("max magnitude = %f, min = %f, allZeros = %d\n", max, min, allZeros);
  return false;
}

void ProcessSamples::ProcessWrite(bool doWrite, 
                                  double centerFrequency,
                                  uint64_t sequenceId)
{
  if (this->m_writing) {
    if (doWrite) {
      this->m_endSequenceId = sequenceId + this->m_postTrigger + 1;
    } else if (sequenceId == this->m_endSequenceId) {
      this->m_sampleQueue->EndWrite(sequenceId);
      this->m_writing = false;
    }
  } else if (doWrite) {
    if (this->m_fileNameBase != "") {
      this->WriteSamplesToFile(sequenceId, centerFrequency);
      this->m_writing = true;
      this->m_endSequenceId = sequenceId + this->m_postTrigger + 1;
    }
  }
}

bool ProcessSamples::StartProcessing(SampleQueue & sampleQueue)
{
  double centerFrequency;
  uint32_t i = 0;
  this->m_sampleQueue = &sampleQueue;
  SampleQueue::MessageType * message;
  this->m_writing = false;
  bool doWrite = false;
  uint64_t sequenceId;
  while (message = sampleQueue.GetNextSamples()) {
    sequenceId = message->GetHeader().m_sequenceId;
    double centerFrequency = message->GetHeader().m_frequency;
    if (this->m_mode == TimeDomain) {
      doWrite = this->DoTimeDomainThresholding(message->GetData(), centerFrequency);
    } else if (this->m_mode == FrequencyDomain) {
      memcpy(this->m_inputSamples, message->GetData(), sizeof(fftwf_complex)*this->m_sampleCount);
      this->m_fftWindow.apply(this->m_inputSamples);
      this->m_fft.process(this->m_fftOutputBuffer, this->m_inputSamples);
      doWrite = this->process_fft(this->m_fftOutputBuffer, centerFrequency);
    }
    if (doWrite) {
      printf(" Sequence Id[%lu]\n", sequenceId);
    }

    this->ProcessWrite(doWrite, centerFrequency, sequenceId);
    sampleQueue.MessageProcessed(message);
  }
  // Shutdown writing gracefully.
  this->m_endSequenceId = sequenceId;
  this->ProcessWrite(false, centerFrequency, sequenceId);
  return true;
}

