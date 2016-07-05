#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <iostream>
#include <limits>
#include <boost/program_options.hpp>
#include "fft.h"
#include "messageQueue.h"
#include "signalSource.h"
#include "process.h"
#include "bladerfSource.h"
#ifdef INCLUDE_B210
#include "b210Source.h"
#endif
#include "airspySource.h"
#include "sdrplaySource.h"
#include "hackRFSource.h"
#include "scan.h"


struct Context
{
  SignalSource * m_signalSource;
  ProcessSamples * m_process;
  SampleQueue * m_sampleQueue;
  struct timespec m_start, m_stop;
} globalContext;

void TerminationHandler(int s)
{
  // printf("Caught signal %d\n",s);
  if (globalContext.m_signalSource != nullptr) {
    globalContext.m_signalSource->StopStreaming();
    globalContext.m_signalSource->Stop();
    fflush(stdout);

    // Calculate and report time.
    clock_gettime(CLOCK_REALTIME, &globalContext.m_stop);
    double startd = globalContext.m_start.tv_sec*1000.0 + globalContext.m_start.tv_nsec/1e6;
    double stopd = globalContext.m_stop.tv_sec*1000.0 + globalContext.m_stop.tv_nsec/1e6;
    double elapsed = stopd - startd;
    fprintf(stderr, "Elapsed time = %f ms\n", elapsed);
    fflush(stderr);

    delete globalContext.m_signalSource;
  }
  exit(1); 
}

int main(int argc, char *argv[])
{
  int status;
  struct module_config config;
  struct bladerf *dev = NULL;
  struct bladerf_devinfo dev_info;
  uint32_t sample_rate = 8000000;
  float threshold;
  double startFrequency;
  double stopFrequency;
  double useBandWidth = 0.75;
  double dcIgnoreWidth = 0.0;
  std::string args;
  std::string spec;
  std::string outFileName;
  std::string modeString;
  uint32_t num_iterations;
  uint32_t sampleCount;
  uint32_t bandWidth;
  uint32_t preTrigger;
  uint32_t postTrigger;
  namespace po = boost::program_options;

  po::options_description desc("Program options");
  desc.add_options()
    ("help", "print help message")
    ("args", po::value<std::string>(&args)->default_value(""), "device args")
    ("bandwidth,b", po::value<uint32_t>(&bandWidth)->default_value(8000000), "Band width")
    ("count,c", po::value<uint32_t>(&sampleCount)->default_value(8192), "sample count")
    ("dcignorewidth,d", po::value<double>(&dcIgnoreWidth)->default_value(0.0), "ignore width window around DC")
    ("mode,m", po::value<std::string>(&modeString)->default_value("time"), "processing mode 'time' or 'frequency'")
    ("niterations,n", po::value<uint32_t>(&num_iterations)->default_value(10), "Number of iterations")
    ("outfile,o", po::value<std::string>(&outFileName)->default_value(""), "File name base to record samples")
    ("pre", po::value<uint32_t>(&preTrigger)->default_value(2), "Pre-trigger buffer save count")
    ("post", po::value<uint32_t>(&postTrigger)->default_value(4), "Post-trigger buffer save count")
    ("samplerate,s", po::value<uint32_t>(&sample_rate)->default_value(8000000), "Sample rate")
    ("spec", po::value<std::string>(&spec)->default_value(""), "Sub-device of UHD device")
    ("threshold,t", po::value<float>(&threshold)->default_value(10.0), "Threshold");

  // Hidden options.
  po::options_description hidden("Hidden options");
  hidden.add_options()
    ("start_freq", po::value<double>(&startFrequency)->default_value(300e6), "Start Frequency")
    ("stop_freq", po::value<double>(&stopFrequency)->default_value(3800e6), "Stop Frequency");

  po::positional_options_description pod;
  pod.add("start_freq", 1);
  pod.add("stop_freq", 2);

  po::options_description command_line_options;
  command_line_options.add(desc).add(hidden);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv)
            .options(command_line_options)
            .positional(pod)
            .run(), 
            vm);
  po::notify(vm);

  ProcessSamples::Mode mode = ProcessSamples::Illegal;
  if (modeString.find("time") != std::string::npos) {
    mode = ProcessSamples::TimeDomain;
  } else if (modeString.find("frequency") != std::string::npos) {
    mode = ProcessSamples::FrequencyDomain;
  }
  if (vm.count("help") || mode == ProcessSamples::Illegal) {
    std::cout << desc << "\n";
    return 1;
  }

  SignalSource * source = nullptr;
  uint32_t enob = 12;
  bool correctDCOffset = false;
  SampleQueue::SampleKind sampleKind = SampleQueue::ShortComplex;
  if (args.find("bladerf") != std::string::npos) {
    source = new BladerfSource(args,
                               sample_rate, 
                               sampleCount, 
                               startFrequency, 
                               stopFrequency,
                               useBandWidth,
                               dcIgnoreWidth);
    correctDCOffset = true;
#ifdef INCLUDE_B210
  } else if (args.find("b200") != std::string::npos) {
    source = new B210Source(args, 
      spec,                      
      sample_rate, 
      sampleCount, 
      startFrequency, 
      stopFrequency);
    sampleKind = SampleQueue::FloatComplex;
#endif
  } else if (args.find("airspy") != std::string::npos) {
    source = new AirspySource(args, 
      sample_rate, 
      sampleCount, 
      startFrequency, 
      stopFrequency);
    sampleKind = SampleQueue::FloatComplex;
    correctDCOffset = false;
  } else if (args.find("sdrplay") != std::string::npos) {
    source = new SdrplaySource(args, 
      sample_rate, 
      sampleCount, 
      startFrequency, 
      stopFrequency,
      bandWidth);
    correctDCOffset = false;
    sampleKind = SampleQueue::Short;
  } else if (args.find("hackrf") != std::string::npos) {
    source = new HackRFSource(args, 
      sample_rate, 
      sampleCount, 
      startFrequency, 
      stopFrequency);
    enob = 8;
    correctDCOffset = true;
    sampleKind = SampleQueue::ByteComplex;
    if (dcIgnoreWidth == 0.0) {
      dcIgnoreWidth = 0.05;
    }
    dcIgnoreWidth = 0.0;
  } else {
    std::cout << "Missing source type argument" << std::endl;
    std::cout << desc << "\n";
    return 1;
  }

  if (source->GetFrequencyCount() > 1) {
    preTrigger = 0;
    postTrigger = 0;
  }

  ProcessSamples process(sampleCount, 
                         sample_rate,
                         enob,
                         threshold, 
                         gr::fft::window::WIN_BLACKMAN_HARRIS,
                         mode,
                         2,
                         outFileName,
                         useBandWidth,
                         dcIgnoreWidth,
                         preTrigger,
                         postTrigger);
  SampleQueue sampleQueue(sampleKind, enob, sampleCount, 1024, correctDCOffset, outFileName != "");

  // Save context and setup termination handler.
  globalContext = Context{source, &process, &sampleQueue};

  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = TerminationHandler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  // Run the system.
  source->Start();
  clock_gettime(CLOCK_REALTIME, &globalContext.m_start);
  source->StartStreaming(num_iterations, sampleQueue);
  process.StartProcessing(sampleQueue);
  TerminationHandler(0);
  return 0;
}



