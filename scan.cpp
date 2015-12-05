#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <libbladeRF.h>
#include <uhd/usrp/multi_usrp.hpp>
#include <libairspy/airspy.h>
#include <math.h>
#include <iostream>
#include <limits>
#include <boost/program_options.hpp>
#include "fft.h"
#include "process.h"
#include "signalSource.h"
#include "bladerfSource.h"
#include "b210Source.h"
#include "airspySource.h"
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
  const uint32_t num_samples = 8192;
  int16_t sample_buffer[num_samples][2];
  float threshold;
  double startFrequency;
  double stopFrequency;
  std::string args;
  uint32_t num_iterations;
  namespace po = boost::program_options;

  po::options_description desc("Program options");
  desc.add_options()
    ("help", "print help message")
    ("args", po::value<std::string>(&args)->default_value(""), "device args")
    ("n_iterations,n", po::value<uint32_t>(&num_iterations)->default_value(10), "Number of iterations")
    ("sample_rate,s", po::value<uint32_t>(&sample_rate)->default_value(8000000), "Sample rate")
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

  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 1;
  }

  SignalSource * source = nullptr;
  uint32_t enob = 12;
  bool correctDCOffset = false;
  bool ignoreCenterFrequency = false;
  if (args.find("bladerf") != std::string::npos) {
    source = new BladerfSource(args,
                               sample_rate, 
                               num_samples, 
                               startFrequency, 
                               stopFrequency);
    correctDCOffset = true;
    ignoreCenterFrequency = true;
  } else if (args.find("b200") != std::string::npos) {
    source = new B210Source(args, 
      sample_rate, 
      num_samples, 
      startFrequency, 
      stopFrequency);
  } else {
    source = new AirspySource(args, 
      sample_rate, 
      num_samples, 
      startFrequency, 
      stopFrequency);
  }

  ProcessSamples process(num_samples, 
                         sample_rate,
                         enob,
                         threshold, 
                         gr::fft::window::WIN_BLACKMAN_HARRIS,
                         correctDCOffset, 
                         ignoreCenterFrequency);

  source->Start();

  struct timespec start, stop;
  clock_gettime(CLOCK_REALTIME, &start);
  double startIter = start.tv_sec*1000.0 + start.tv_nsec/1e6;

  double previousFrequency = startFrequency;
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
    if (centerFrequency < previousFrequency) {
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



