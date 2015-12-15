#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <iostream>
#include <limits>
#include <cassert>
#include <volk/volk.h>
#include "fft.h"
#include "signalSource.h"
#include "process.h"
#include "buffer.cpp"

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

void ProcessSamples::short_complex_to_float_complex(int16_t source[][2],
                                                    fftwf_complex * destination)
{
  int16_t max = 1 << (this->m_enob - 1);
  float onebymax = float(1.0/max);
  int32_t dc_real = 0;
  int32_t dc_imag = 0;
  if (this->m_correctDCOffset) {
    for (uint32_t i = 0; i < this->m_sampleCount; i++) {
      dc_real += source[i][0];
      dc_imag += source[i][1];
    }
    dc_real /= this->m_sampleCount;
    dc_imag /= this->m_sampleCount;
  }
  for (uint32_t i = 0; i < this->m_sampleCount; i++) {
    destination[i][0] = float(source[i][0] - dc_real)*onebymax;
    destination[i][1] = float(source[i][1] - dc_imag)*onebymax;
  }
}


void ProcessSamples::complex_to_magnitude(fftwf_complex * fft_data, 
                                          float * magnitudes)
{
  for (uint32_t i = 0; i < this->m_sampleCount; i++) {
    float re = fft_data[i][0];
    float im = fft_data[i][1];
    float mag = sqrt(re * re + im * im) / this->m_sampleCount;
    magnitudes[i] = 10 * log2(mag) / log2(10);
  }
}

void ProcessSamples::process_fft(fftwf_complex * fft_data, 
                                 uint32_t center_frequency)
{
  uint32_t start_frequency = center_frequency - this->m_sampleRate/2;
  uint32_t bin_step = this->m_sampleRate/this->m_sampleCount;
  float magnitudes[this->m_sampleCount];

  complex_to_magnitude(fft_data, magnitudes);

  for (uint32_t i = 0; i < this->m_sampleCount; i++) {
    uint32_t j = (i + this->m_sampleCount/2) % this->m_sampleCount;
    if (j < this->m_dcIgnoreWindow || (this->m_sampleCount - j < this->m_dcIgnoreWindow)) {
      continue;
    }
    if (magnitudes[j] > this->m_threshold) {
      uint32_t frequency = start_frequency + i*bin_step;
      printf("freq %u power_db %f\n", frequency, magnitudes[j]);
    }
  }
}

ProcessSamples::ProcessSamples(uint32_t numSamples, 
                               uint32_t sampleRate, 
                               uint32_t enob,
                               float threshold,
                               gr::fft::window::win_type windowType,
                               bool correctDCOffset,
                               uint32_t dcIgnoreWindow,
                               std::string outFileName,
                               bool doFFT)
  : m_sampleCount(numSamples),
    m_sampleRate(sampleRate),
    m_enob(enob),
    m_fileCounter(0),
    m_threshold(threshold),
    m_fftWindow(windowType, numSamples),
    m_correctDCOffset(correctDCOffset),
    m_dcIgnoreWindow(dcIgnoreWindow),
    m_fft(numSamples),
    m_circularBuffer(16, nullptr),
    m_writeInterface(!outFileName.empty() ? outFileName.c_str() : nullptr),
    m_writeSamples(!outFileName.empty()),
    m_doFFT(doFFT)
{
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
  this->short_complex_to_float_complex(sample_buffer, this->m_inputSamples);
  this->m_circularBuffer.AppendItems(this->m_inputSamples, this->m_sampleCount);
  if (this->m_doFFT) {
    this->m_fftWindow.apply(this->m_inputSamples);
    this->m_fft.process(this->m_fftOutputBuffer, this->m_inputSamples);
    this->process_fft(this->m_fftOutputBuffer, centerFrequency);
  }
}

std::string ProcessSamples::GenerateFileName(time_t startTime, 
                                             double_t centerFrequency)
{
  char buffer[256];
  const char timeformat[] = "%Y%m%d-%T";
  struct tm * timeStruct = localtime(&startTime);
  if (timeStruct == nullptr) {
    perror("localtime");
    exit(1);
  }
  if (strftime(buffer, sizeof(buffer), timeformat, timeStruct) == 0) {
    fprintf(stderr, "strftime returned 0");
    exit(1);
  }
  sprintf(buffer+strlen(buffer), "-%.0f-%u", centerFrequency, ++this->m_fileCounter);
  return std::string(buffer);
}

void ProcessSamples::WriteSamplesToFile(std::string fileName, uint32_t count)
{
  fprintf(stderr, "Writing to file %s\n", fileName.c_str());
  FileWriteProcessInterface writeInterface(fileName.c_str());
  this->m_circularBuffer.ProcessItems(count, &writeInterface);
}

void ProcessSamples::WriteSamples(uint32_t count)
{
  if (this->m_writeSamples) {
    this->m_circularBuffer.ProcessItems(count, &this->m_writeInterface);
  }
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
    signalSource->GetNextSamples(sample_buffer, centerFrequency);
    this->short_complex_to_float_complex(sample_buffer, this->m_inputSamples);
    this->m_circularBuffer.AppendItems(this->m_inputSamples, this->m_sampleCount);
    // Check for threshold.
    std::string fileName = this->GenerateFileName(startTime, centerFrequency);
    this->WriteSamplesToFile(fileName, 2 * this->m_sampleCount);
   }
}
