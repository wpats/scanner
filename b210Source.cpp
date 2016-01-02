
#include <uhd/usrp/multi_usrp.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <complex>
#include "messageQueue.h"
#include "signalSource.h"
#include "b210Source.h"

#define HANDLE_ERROR(format, ...) this->handle_error(this->m_dev, status, format, ##__VA_ARGS__)

B210Source::B210Source(std::string args,
                       uint32_t sampleRate, 
                       uint32_t sampleCount, 
                       double startFrequency, 
                       double stopFrequency)
  : SignalSource(sampleRate, sampleCount, startFrequency, stopFrequency),
    m_verbose(false)
{
  this->m_frequencyIncrement = sampleRate;
  std::cout << "Frequency increment = " << this->m_frequencyIncrement << std::endl;
  //create a usrp device
  std::cout << std::endl;
  std::cout << boost::format("Creating the usrp device with: %s...") % args << std::endl;
  this->m_usrp = uhd::usrp::multi_usrp::make(args);
  std::cout << boost::format("Using Device: %s") % this->m_usrp->get_pp_string() << std::endl;

  //detect which channels to use.
  // Use channel 0.
  std::vector<size_t> channel_nums;
  channel_nums.push_back(0);

  //set the rx sample rate
  std::cout << boost::format("Setting RX Rate: %f Msps...") % (sampleRate/1e6) << std::endl;
  this->m_usrp->set_rx_rate(sampleRate);
  std::cout << boost::format("Actual RX Rate: %f Msps...") % (this->m_usrp->get_rx_rate()/1e6) << std::endl << std::endl;

  std::cout << boost::format("Setting device timestamp to 0...") << std::endl;
  this->m_usrp->set_time_now(uhd::time_spec_t(0.0));


  if (this->m_verbose) {
    std::vector<std::string> gain_names = this->m_usrp->get_rx_gain_names(0);
    for (auto & name : gain_names) {
      std::cout << "Gain name " << name << std::endl;
    }
  }
  this->m_usrp->set_rx_gain(38.0, 0);

  //create a receive streamer
  // uhd::stream_args_t stream_args("sc16", "sc16"); //complex floats
  uhd::stream_args_t stream_args("fc32", "sc16"); //complex floats
  stream_args.channels = channel_nums;
  this->m_rx_stream = this->m_usrp->get_rx_stream(stream_args);

  // Tune to the first frequency.
  this->Retune(this->GetCurrentFrequency());

  // Sleep until the device is ready to stream samples.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
}


B210Source::~B210Source()
{

}

double B210Source::Retune(double currentFrequency)
{
  //advanced tuning with tune_request_t uhd::tune_request_t
  uhd::tune_request_t tune_req(currentFrequency, 0);
  tune_req.args = uhd::device_addr_t("mode_n=integer"); //to use Int-N tuning
  //fill in any additional/optional tune request fields...
  tune_req.dsp_freq_policy = uhd::tune_request_t::POLICY_AUTO;
  tune_req.rf_freq = currentFrequency;
  tune_req.rf_freq_policy = uhd::tune_request_t::POLICY_MANUAL;
  this->StartTimer();
  this->m_usrp->set_rx_freq(tune_req);
  while (not this->m_usrp->get_rx_sensor("lo_locked").to_bool()) {
    //sleep for a short time in milliseconds
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  this->StopTimer();
  this->AddRetuneTime();
  if (this->m_verbose) {
    std::cout << "Tuned to " << currentFrequency << std::endl;
  }
  return currentFrequency;
}

bool B210Source::GetNextSamples(SampleQueue * sampleQueue, double & centerFrequency)
{
  // Retune and set the centerFrequency.
  centerFrequency = this->GetCurrentFrequency();

  //setup streaming
  uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_DONE);
  stream_cmd.num_samps = this->m_sampleCount;
  stream_cmd.stream_now = true;
  stream_cmd.time_spec = uhd::time_spec_t(0.0);
  this->m_rx_stream->issue_stream_cmd(stream_cmd);

  std::vector<void *> buffs(2);
  int16_t sample_buffer[this->m_sampleCount][2];
  // meta-data will be filled in by recv()
  uhd::rx_metadata_t md;
  double timeout = 0.1; //timeout (delay before receive + padding)
  uint32_t nSamples = 0; //number of accumulated samples
  while(nSamples < this->m_sampleCount) {
    // setup the buffer.
     buffs[0] = &sample_buffer[nSamples][0];
    //receive a single packet
    this->StartTimer();
    size_t num_rx_samps = this->m_rx_stream->recv(buffs, 
                                                  this->m_sampleCount - nSamples,
                                                  md, 
                                                  timeout, 
                                                  true
                                                  );

    //handle the error code
    if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_TIMEOUT) break;
    if (md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE){
      throw std::runtime_error(str(boost::format("Receiver error %s"
                                                 ) % md.strerror()));
    }
    this->StopTimer();
    this->AddGetSamplesTime();
    if (this->m_verbose) {
      std::cout << boost::format(
            "Received packet: %u samples, %u full secs, %f frac secs"
        ) % num_rx_samps % md.time_spec.get_full_secs() % md.time_spec.get_frac_secs() << std::endl;
    }
    nSamples += num_rx_samps;
  }

  if (this->GetFrequencyCount() > 1) {
    this->Retune(this->GetNextFrequency());
  }
  if (nSamples < this->m_sampleCount) {
    std::cerr << "Receive timeout before all samples received..." << std::endl;
    return false;
  }

  if (this->m_verbose) {
    std::cout << "Done!" << std::endl;
  }
  
  return true;
}

bool B210Source::StartStreaming(uint32_t numIterations, SampleQueue & sampleQueue)
{
  this->m_iterationCount = numIterations;
  this->m_sampleQueue = &sampleQueue;
  auto result = this->StartThread();
  return result;
}

void B210Source::ThreadWorker()
{
  while (this->GetIterationCount() > 0) {
    // Set the centerFrequency.
    double centerFrequency = this->GetCurrentFrequency();

    //setup streaming
    uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_DONE);
    stream_cmd.num_samps = this->m_sampleCount;
    stream_cmd.stream_now = true;
    stream_cmd.time_spec = uhd::time_spec_t(0.0);
    this->m_rx_stream->issue_stream_cmd(stream_cmd);

    std::vector<void *> buffs(2);
    // int16_t sample_buffer[this->m_sampleCount][2];
    fftwf_complex sample_buffer[this->m_sampleCount];
    // meta-data will be filled in by recv()
    uhd::rx_metadata_t md;
    double timeout = 0.1; //timeout (delay before receive + padding)
    uint32_t nSamples = 0; //number of accumulated samples
    while(nSamples < this->m_sampleCount) {
      // setup the buffer.
      buffs[0] = &sample_buffer[nSamples][0];
      //receive a single packet
      this->StartTimer();
      size_t num_rx_samps = this->m_rx_stream->recv(buffs, 
                                                    this->m_sampleCount - nSamples,
                                                    md, 
                                                    timeout, 
                                                    true
                                                    );

      //handle the error code
      if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_TIMEOUT) break;
      if (md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE){
        throw std::runtime_error(str(boost::format("Receiver error %s"
                                                   ) % md.strerror()));
      }
      this->StopTimer();
      this->AddGetSamplesTime();
      if (this->m_verbose) {
        std::cout << boost::format(
                                   "Received packet: %u samples, %u full secs, %f frac secs"
                                   ) % num_rx_samps % md.time_spec.get_full_secs() % md.time_spec.get_frac_secs() << std::endl;
      }
      nSamples += num_rx_samps;
    }
    if (nSamples < this->m_sampleCount) {
      std::cerr << "Receive timeout before all samples received..." << std::endl;
      exit(1);
    }
    double nextFrequency = this->GetNextFrequency();
    if (this->GetFrequencyCount() > 1) {
      this->Retune(nextFrequency);
    }
    this->m_sampleQueue->AppendSamples(sample_buffer, centerFrequency);
  }
}
