#include <vector>
#include <stdio.h>
#include <memory.h>
#include <assert.h>
#include <stdlib.h>
#include "fft.h"
#include "buffer.h"

FileWriteProcessInterface::FileWriteProcessInterface(const char * outFileName)
  : m_count(0),
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
  size_t written = fwrite(items, 
                          sizeof(fftwf_complex), 
                          count, 
                          this->m_outFile);
  if (written != count) {
    fprintf(stderr, "Error writing to file\n");
    exit(1);
  }
  this->m_count += written;

#if 0
  for (uint32_t i = 0; i < count; i++) {
    fprintf(this->m_outFile, "%f, %f\n", items[i][0], items[i][1]);
  }
#endif
}
    
void FileWriteProcessInterface::End()
{
  assert(this->m_expectedCount == this->m_count);
  this->m_count = this->m_expectedCount = 0;
}
