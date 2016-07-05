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

bool ProcessSamples::process_fft(fftwf_complex * fft_data, SampleQueue::MessageHeader * header)
{
  uint32_t start_frequency = header->m_frequency - this->m_sampleRate/2;
  uint32_t bin_step = this->m_sampleRate/this->m_sampleCount;
  float magnitudes[this->m_sampleCount];

  Utility::complex_to_magnitude(fft_data, magnitudes, this->m_sampleCount);
  bool trigger = false;
  uint32_t halfSampleCount = this->m_sampleCount/2;
  for (uint32_t i = 0; i < this->m_sampleCount; i++) {
    uint32_t j = (i + halfSampleCount) % this->m_sampleCount;
    if (i < this->m_dcIgnoreWindow || (this->m_sampleCount - i) < this->m_dcIgnoreWindow) {
      continue;
    }
    if (i > this->m_useWindow && i < (this->m_sampleCount - this->m_useWindow)) {
      continue;
    }
    if (magnitudes[j] > this->m_threshold) {
      uint32_t frequency = start_frequency + i*bin_step;
      // printf("Sequence[%llu] ", header->m_sequenceId);
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
                               Mode mode,
                               uint32_t threadCount,
                               std::string fileNameBase,
                               double useBandWidth,
                               double dcIgnoreWidth,
                               uint32_t preTrigger,
                               uint32_t postTrigger)
  : m_sampleCount(numSamples),
    m_sampleRate(sampleRate),
    m_enob(enob),
    m_fileCounter(0),
    m_threshold(threshold),
    m_fftWindow(windowType, numSamples),
    m_correctDCOffset(false),
    m_useWindow(uint32_t(useBandWidth * numSamples / 2.0)),
    m_dcIgnoreWindow(uint32_t(dcIgnoreWidth * numSamples / 2.0)),
    m_fft(numSamples),
    m_mode(mode),
    m_sampleQueue(nullptr),
    m_fileNameBase(fileNameBase),
    m_preTrigger(preTrigger),
    m_postTrigger(postTrigger),
    m_writing(false),
    m_endSequenceId(0),
    m_threadCount(threadCount)
{
  assert(mode > Illegal && mode <= FrequencyDomain);
  assert(threadCount <= MAX_THREADS);
  for (uint32_t threadId = 0; threadId < threadCount; threadId++) {
    this->m_inputSamples[threadId] = 
      reinterpret_cast<fftwf_complex *>(fftwf_alloc_complex(numSamples));
    this->m_fftOutputBuffer[threadId] = 
      reinterpret_cast<fftwf_complex *>(fftwf_alloc_complex(numSamples));
    this->m_threads[threadId] = nullptr;
  }
}

ProcessSamples::~ProcessSamples()
{
  for (uint32_t threadId = 0; threadId < this->m_threadCount; threadId++) {
    fftwf_free(this->m_inputSamples[threadId]);
    fftwf_free(this->m_fftOutputBuffer[threadId]);
  }
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
                                          this->m_inputSamples[0],
                                          this->m_sampleCount,
                                          this->m_enob,
                                          this->m_correctDCOffset);
  if (this->m_mode == FrequencyDomain) {
    this->m_fftWindow.apply(this->m_inputSamples[0]);
    this->m_fft.process(this->m_fftOutputBuffer[0], this->m_inputSamples[0]);
    // TODO: Materialize a MessageHeader struct here.
    this->process_fft(this->m_fftOutputBuffer[0], nullptr);
  }
}

void ProcessSamples::TimeToString(time_t time, char * buffer, uint32_t length)
{
  const char timeformat[] = "%Y%m%d-%T";
  struct tm * timeStruct = localtime(&time);
  if (timeStruct == nullptr) {
    perror("localtime");
    exit(1);
  }
  if (strftime(buffer, length, timeformat, timeStruct) == 0) {
    fprintf(stderr, "strftime returned 0");
    exit(1);
  }
}

std::string ProcessSamples::GenerateFileName(std::string fileNameBase, 
                                             time_t startTime, 
                                             double_t centerFrequency)
{
  char buffer[256];
  char timeBuffer[64];
  strcpy(buffer, fileNameBase.c_str());
  this->TimeToString(startTime, timeBuffer, std::extent<decltype(timeBuffer)>::value);
  strcat(buffer, timeBuffer);
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
                                            this->m_inputSamples[0],
                                            this->m_sampleCount,
                                            this->m_enob,
                                            this->m_correctDCOffset);
    // Check for threshold.
    std::string fileName = this->GenerateFileName(this->m_fileNameBase, startTime, centerFrequency);
    // this->WriteSamplesToFile(fileName, this->m_sampleCount);
   }
}

bool ProcessSamples::DoTimeDomainThresholding(fftwf_complex * inputSamples,
                                              SampleQueue::MessageHeader * header)
{
  double log10 = log2(10);
  float maxMagnitude = std::numeric_limits<float>::min();
  float minMagnitude = std::numeric_limits<float>::max();
  bool allZeros = true;
  float maxre = std::numeric_limits<float>::min();
  float maxim = std::numeric_limits<float>::min();
  for (uint32_t i = 0; i < this->m_sampleCount; i++) {
    float re = inputSamples[i][0];
    float im = inputSamples[i][1];
    if (re != 0.0 || im != 0.0) {
      allZeros = false;
    }
    maxre = std::max(maxre, re);
    maxim = std::max(maxim, im);
    float mag = sqrt(re * re + im * im); // / this->m_sampleCount;
    float magnitude = 10 * log2(mag) / log10;
    maxMagnitude = std::max(maxMagnitude, magnitude);
    minMagnitude = std::min(minMagnitude, magnitude);
  }  
  
  if (maxMagnitude >= this->m_threshold) {
    printf("Sequence[%llu]: ", header->m_sequenceId);
    printf("Max signal %f above threshold %f frequency %.0f, min %f\n", 
           maxMagnitude, 
           this->m_threshold,
           header->m_frequency,
           minMagnitude);
    printf("Max re[%f], im[%f]\n", maxre, maxim);
    return true;
  }
  return false;
}

void ProcessSamples::UpdateEndSequenceId(uint64_t newEndSequenceId)
{
  while (true) {
    uint64_t currentId = this->m_endSequenceId;
    uint64_t maxId = std::max<uint64_t>(newEndSequenceId, currentId);
    if (this->m_endSequenceId.compare_exchange_strong(currentId, maxId)) {
      break;
    }
  }
}

void ProcessSamples::ProcessWrite(bool doWrite, 
                                  double centerFrequency,
                                  uint64_t sequenceId)
{
  if (this->m_writing) {
    if (doWrite) {
      uint64_t newEndSequenceId = sequenceId + this->m_postTrigger + 1;
      this->UpdateEndSequenceId(newEndSequenceId);
    } else if (sequenceId == this->m_endSequenceId) {
      this->m_sampleQueue->EndWrite(sequenceId);
      this->m_writing = false;
    }
  } else if (doWrite) {
    if (this->m_fileNameBase != "") {
      this->WriteSamplesToFile(sequenceId, centerFrequency);
      this->m_writing = true;
      uint64_t newEndSequenceId = sequenceId + this->m_postTrigger + 1;
      this->UpdateEndSequenceId(newEndSequenceId);
    }
  }
}

void ProcessSamples::ThreadWorker(uint32_t threadId)
{
  double centerFrequency;
  uint32_t i = 0;
  SampleQueue::MessageType * message;
  bool doWrite = false;
  uint64_t sequenceId;
  while (message = this->m_sampleQueue->GetNextSamples()) {
    if (message->m_header.m_time != 0) {
      char timeBuffer[64];
      this->TimeToString(message->m_header.m_time, 
                         timeBuffer, 
                         std::extent<decltype(timeBuffer)>::value);
      printf("Start scan at %s\n", timeBuffer);
      fflush(stdout);
    }
    sequenceId = message->GetHeader().m_sequenceId;
    double centerFrequency = message->GetHeader().m_frequency;
    if (this->m_mode == TimeDomain) {
      doWrite = this->DoTimeDomainThresholding(message->GetData(), &message->m_header);
    } else if (this->m_mode == FrequencyDomain) {
      memcpy(this->m_inputSamples[threadId], 
             message->GetData(), 
             sizeof(fftwf_complex)*this->m_sampleCount);
      this->m_fftWindow.apply(this->m_inputSamples[threadId]);
      this->m_fft.process(this->m_fftOutputBuffer[threadId], 
                          this->m_inputSamples[threadId]);
      doWrite = this->process_fft(this->m_fftOutputBuffer[threadId], &message->m_header);
    }
    if (doWrite) {
      fflush(stdout);
    }
    this->ProcessWrite(doWrite, centerFrequency, sequenceId);
    this->m_sampleQueue->MessageProcessed(message);
  }
  // Shutdown writing gracefully.
  this->UpdateEndSequenceId(sequenceId);
  this->ProcessWrite(false, centerFrequency, sequenceId);
}

bool ProcessSamples::StartProcessing(SampleQueue & sampleQueue)
{
  this->m_sampleQueue = &sampleQueue;
  for (uint32_t threadId = 0; threadId < this->m_threadCount; threadId++) {
    printf("Starting process thread %u\n", threadId);
    this->m_threads[threadId] = new std::thread(&ProcessSamples::ThreadWorker, 
                                                this,
                                                threadId);
  }
  for (uint32_t threadId = 0; threadId < this->m_threadCount; threadId++) {
    this->m_threads[threadId]->join();
    printf("Stopped process thread %u\n", threadId);
  }

  return true;
}

