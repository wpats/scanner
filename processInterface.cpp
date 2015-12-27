#include <vector>
#include <stdio.h>
#include <memory.h>
#include <assert.h>
#include <stdlib.h>
#include "fft.h"
#include "buffer.h"

FileWriteProcessInterface::FileWriteProcessInterface(const char * outFileName)
  : ProcessInterface(true),
    m_count(0),
    m_expectedCount(0)
{
  if (outFileName != NULL) {
    this->m_outFile = fopen(outFileName, "w");
    if (this->m_outFile == NULL) {
      fprintf(stderr, "Error opening file '%s'\n", outFileName);
      exit(1);
    }
  }
}

FileWriteProcessInterface::~FileWriteProcessInterface()
{
  if (this->m_outFile != NULL) {
    fclose(this->m_outFile);
  }
}

void FileWriteProcessInterface::Begin(uint64_t sequenceId, uint32_t totalItemCount)
{
  this->m_expectedCount = totalItemCount;
}

void FileWriteProcessInterface::Process(const fftwf_complex * items, uint32_t count)
{
#if 1
  size_t written = fwrite(items, 
                          sizeof(fftwf_complex), 
                          count, 
                          this->m_outFile);
  if (written != count) {
    fprintf(stderr, "Error writing to file\n");
    exit(1);
  }
  this->m_count += written;
#else
  for (uint32_t i = 0; i < count; i++) {
    fprintf(this->m_outFile, "%f, %f\n", items[i][0], items[i][1]);
  }
  this->m_count += count;
#endif
}
    
void FileWriteProcessInterface::End()
{
  assert(this->m_expectedCount == this->m_count);
  this->m_count = this->m_expectedCount = 0;
}

// CopyBufferProcessInterface methods.
//
CopyBufferProcessInterface::CopyBufferProcessInterface(fftwf_complex * outputBuffer)
  : ProcessInterface(false),
    m_count(0),
    m_expectedCount(0),
    m_outputBuffer(outputBuffer)
{
}

CopyBufferProcessInterface::~CopyBufferProcessInterface()
{
}

void CopyBufferProcessInterface::Begin(uint64_t sequenceId, uint32_t totalItemCount)
{
  this->m_expectedCount = totalItemCount;
}

void CopyBufferProcessInterface::Process(const fftwf_complex * items, uint32_t count)
{
  memcpy(this->m_outputBuffer + this->m_count, items, sizeof(fftwf_complex) * count);
  this->m_count += count;
}
    
void CopyBufferProcessInterface::End()
{
  assert(this->m_expectedCount == this->m_count);
  this->m_count = this->m_expectedCount = 0;
}
