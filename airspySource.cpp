
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <iostream>
#include <limits>
#include <stdarg.h>
#include <cassert>
#include <vector>
#include <algorithm>
#include <libairspy/airspy.h>
#include "signalSource.h"
#include "airspySource.h"

#define HANDLE_ERROR(format, ...) this->handle_error(status, format, ##__VA_ARGS__)

void AirspySource::handle_error(int status, const char * format, ...)
{
  if (status != 0) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    fprintf(stderr, buffer, airspy_error_name(static_cast<airspy_error>(status)));
    airspy_close(this->m_dev);
    exit(1);
  }
}

AirspySource::AirspySource(std::string args,
                           uint32_t sampleRate, 
                           uint32_t sampleCount, 
                           double startFrequency, 
                           double stopFrequency)
  : m_sampleRate(sampleRate),
    m_sampleCount(sampleCount),
    m_frequencyIndex(0),
    m_dev(nullptr),
    m_streamingState(Illegal)
{
  int status;

  status = airspy_open( &this->m_dev );
  HANDLE_ERROR("Failed to open AirSpy device: %%s\n");

  uint8_t board_id;
  status = airspy_board_id_read( this->m_dev, &board_id );
  HANDLE_ERROR("Failed to get AirSpy board id: %%s\n");

  char version[128];
  memset(version, 0, sizeof(version));
  status = airspy_version_string_read( this->m_dev, version, sizeof(version));
  HANDLE_ERROR("Failed to read version string: %%s\n");

#if 0
  read_partid_serialno_t serial_number;
  ret = airspy_board_partid_serialno_read( _dev, &serial_number );
  AIRSPY_THROW_ON_ERROR(ret, "Failed to read serial number");
#endif
  
  this->set_sample_rate(sampleRate);
    
  /* Parameter value shall be between 0 and 15 */
  airspy_set_lna_gain(this->m_dev, 7);

  /* Parameter value shall be between 0 and 15 */
  airspy_set_mixer_gain(this->m_dev, 7);

  /* Parameter value shall be between 0 and 15 */
  airspy_set_vga_gain(this->m_dev, 7);

  /* Parameter value:
     0=Disable LNA Automatic Gain Control
     1=Enable LNA Automatic Gain Control
  */
  airspy_set_lna_agc(this->m_dev, 0);

  /* Parameter value:
     0=Disable MIXER Automatic Gain Control
     1=Enable MIXER Automatic Gain Control
  */
  airspy_set_mixer_agc(this->m_dev, 0);

  /* Parameter value shall be 0=Disable BiasT or 1=Enable BiasT */
  airspy_set_rf_bias(this->m_dev, 0);

  if (args.find("bias")) {
    status = airspy_set_rf_bias(this->m_dev, 1);
    HANDLE_ERROR("Failed to enable DC bias: %%s\n");
  }

  status = airspy_set_sample_type(this->m_dev, AIRSPY_SAMPLE_INT16_IQ);
  HANDLE_ERROR("Failed to set sample type: %%s\n");

  this->m_numFrequencies = (stopFrequency - startFrequency)/sampleRate + 1;
  this->m_frequencies = new uint32_t[this->m_numFrequencies];
  for (uint32_t i = 0; i < this->m_numFrequencies; i++) {
    this->m_frequencies[i] = startFrequency + i * sampleRate;
    // fprintf(stderr, "Frequency %d: %u\n", i, this->m_frequencies[i]);
  }

  double centerFrequency = this->m_frequencies[this->m_frequencyIndex];
  status = airspy_set_freq(this->m_dev, centerFrequency);
  HANDLE_ERROR("Failed to tune  %u Hz: %%s\n", 
               centerFrequency);
  status = airspy_start_rx(this->m_dev, _airspy_rx_callback, (void *)this );
  HANDLE_ERROR("Failed to start RX streaming at %u: %%s\n", centerFrequency);
}

AirspySource::~AirspySource()
{
  if (this->m_dev != nullptr) {
    int status = airspy_stop_rx(this->m_dev);
    double centerFrequency = this->m_frequencies[this->m_frequencyIndex];
    HANDLE_ERROR("Failed to stop RX streaming at %u: %%s\n", centerFrequency);
    status = airspy_close(this->m_dev);
    HANDLE_ERROR("Error closing airspy: %%s\n");
  }
}

double AirspySource::set_sample_rate( double rate )
{
  assert(this->m_dev != nullptr);

  int status = AIRSPY_SUCCESS;
  std::vector< std::pair<double, uint32_t> > _sample_rates;
  uint32_t num_rates;
  airspy_get_samplerates(this->m_dev, &num_rates, 0);
  uint32_t *samplerates = (uint32_t *) malloc(num_rates * sizeof(uint32_t));
  airspy_get_samplerates(this->m_dev, samplerates, num_rates);
  for (size_t i = 0; i < num_rates; i++)
    _sample_rates.push_back( std::pair<double, uint32_t>( samplerates[i], i ) );
  free(samplerates);

  /* since they may (and will) give us an unsorted array we have to sort it here
   * to play nice with the monotonic requirement of meta-range later on */
  std::sort(_sample_rates.begin(), _sample_rates.end());
#if 0
  std::cerr << "Samplerates: ";
  for (size_t i = 0; i < _sample_rates.size(); i++)
    std::cerr << boost::format("%gM ") % (_sample_rates[i].first / 1e6);
  std::cerr << std::endl;
#endif

  bool found_supported_rate = false;
  uint32_t samp_rate_index = 0;
  for( unsigned int i = 0; i < _sample_rates.size(); i++ ) {
    if( _sample_rates[i].first == rate ) {
      samp_rate_index = _sample_rates[i].second;
      found_supported_rate = true;
    }
  }

  if ( ! found_supported_rate ) {
    status = AIRSPY_ERROR_OTHER;
    HANDLE_ERROR("Unsupported samplerate: %gM", rate/1e6);
  }

  status = airspy_set_samplerate( this->m_dev, samp_rate_index );
  HANDLE_ERROR("Error setting sample rate: %%s\n");
}

int AirspySource::_airspy_rx_callback(airspy_transfer* transfer)
{
  AirspySource * obj = (AirspySource *)transfer->ctx;
  return obj->airspy_rx_callback((float *)transfer->samples, transfer->sample_count);
}

int AirspySource::airspy_rx_callback(void *samples, int sample_count)
{
  if (this->m_streamingState == GetSamples) {
    memcpy(this->m_sample_buffer, samples, sizeof(int16_t) * 2 * this->m_sampleCount);
    this->m_streamingState = GotSamples;
  }
  return 0; // TODO: return -1 on error/stop
}

bool AirspySource::GetNextSamples(int16_t sample_buffer[][2], double & centerFrequency)
{
  /* Tune to the specified frequency immediately via BLADERF_RETUNE_NOW.
   *
   * Alternatively, this re-tune could be scheduled by providing a
   * timestamp counter value */
  int status;
  uint32_t delta = 100;
  centerFrequency = this->m_frequencies[this->m_frequencyIndex];
  status = airspy_set_freq(this->m_dev, centerFrequency);
  HANDLE_ERROR("Failed to tune  %u Hz: %%s\n", 
               centerFrequency);

  // sleep(0.010);
  //fprintf(stderr, "Tuned to %u\n", frequencies[j]);
  /* ... Handle signals at current frequency ... */
  this->m_sample_buffer = sample_buffer;
  this->m_streamingState = GetSamples;

  while (this->m_streamingState != GotSamples) {
  }

  this->m_frequencyIndex = (this->m_frequencyIndex + 1) % this->m_numFrequencies;
  return true;
}
