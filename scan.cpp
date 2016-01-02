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
#include "scan.h"



/* Usage:
 * libbladeRF_example_boilerplate [serial #]
 *
 * If a serial number is supplied, the program will attempt to open the
 * device with the provided serial number.
 *
 * Otherwise, the first available device will be used.
 */
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
  std::string args;
  std::string outFileName;
  std::string modeString;
  uint32_t num_iterations;
  uint32_t dcIgnoreWindow = 0;
  uint32_t sampleCount;
  uint32_t bandWidth;
  namespace po = boost::program_options;

  po::options_description desc("Program options");
  desc.add_options()
    ("help", "print help message")
    ("args", po::value<std::string>(&args)->default_value(""), "device args")
    ("bandwidth,b", po::value<uint32_t>(&bandWidth)->default_value(8000000), "Band width")
    ("count,c", po::value<uint32_t>(&sampleCount)->default_value(8192), "sample count")
    ("dcwindow,d", po::value<uint32_t>(&dcIgnoreWindow)->default_value(0), "ignore window around DC")
    ("mode,m", po::value<std::string>(&modeString)->default_value("time"), "processing mode 'time' or 'frequency'")
    ("niterations,n", po::value<uint32_t>(&num_iterations)->default_value(10), "Number of iterations")
    ("outfile,o", po::value<std::string>(&outFileName)->default_value(""), "File name base to record samples")
    ("samplerate,s", po::value<uint32_t>(&sample_rate)->default_value(8000000), "Sample rate")
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
                               stopFrequency);
    correctDCOffset = true;
    if (dcIgnoreWindow == 0) {
      dcIgnoreWindow = 7;
    }
#ifdef INCLUDE_B210
  } else if (args.find("b200") != std::string::npos) {
    source = new B210Source(args, 
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
    if (dcIgnoreWindow == 0) {
      dcIgnoreWindow = 24;
    }
  } else if (args.find("sdrplay") != std::string::npos) {
    source = new SdrplaySource(args, 
      sample_rate, 
      sampleCount, 
      startFrequency, 
      stopFrequency,
      bandWidth);
    correctDCOffset = true;
    sampleKind = SampleQueue::Short;
  } else {
    std::cout << "Missing source type argument" << std::endl;
    std::cout << desc << "\n";
    return 1;
  }

  ProcessSamples process(sampleCount, 
                         sample_rate,
                         enob,
                         threshold, 
                         gr::fft::window::WIN_BLACKMAN_HARRIS,
                         correctDCOffset, 
                         mode,
                         outFileName,
                         dcIgnoreWindow);
  SampleQueue sampleQueue(sampleKind, enob, sampleCount, 1024, outFileName != "");
  source->Start();

  struct timespec start, stop;
  clock_gettime(CLOCK_REALTIME, &start);
  double startIter = start.tv_sec*1000.0 + start.tv_nsec/1e6;

  source->StartStreaming(num_iterations, sampleQueue);
  process.StartProcessing(sampleQueue);
  source->StopStreaming();

#if 0
  double previousFrequency = startFrequency;
  uint32_t frequencyCount = source->GetFrequencyCount();
  for (uint32_t i = 0; i < num_iterations;) {
    double centerFrequency;
    if (!source->GetNextSamples(sample_buffer, centerFrequency)) {
      fprintf(stderr, "Receive samples failed, exiting...\n");
      break;
    }
    source->WriteTimingData();
    // Process samples
    process.Run(sample_buffer, centerFrequency);

    // Detect complete scan.
    if (--frequencyCount == 0) {
      frequencyCount = source->GetFrequencyCount();
      i++;

#if 1
      // Calculate and report time.
      if (i % 1 == 0) {
        struct timespec iterStop;
        clock_gettime(CLOCK_REALTIME, &iterStop);
        double stopd = iterStop.tv_sec*1000.0 + iterStop.tv_nsec/1e6;
        double elapsed = stopd - startIter;
        fprintf(stderr, "Iteration time = %f ms\n", elapsed/1.0);
        startIter = stopd;
      }
#endif
    }
    previousFrequency = centerFrequency;
  }
#else
  // process.RecordSamples(source, sampleCount * num_iterations, 0.0);
#endif

  source->Stop();
  // Calculate and report time.
  clock_gettime(CLOCK_REALTIME, &stop);
  double startd = start.tv_sec*1000.0 + start.tv_nsec/1e6;
  double stopd = stop.tv_sec*1000.0 + stop.tv_nsec/1e6;
  double elapsed = stopd - startd;
  fprintf(stderr, "Elapsed time = %f ms\n", elapsed);

  delete source;
  return 0;
}



