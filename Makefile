
scan: scan.o fft.o
	g++ -g -o scan scan.o fft.o  -L ../target/lib -lbladeRF -L /usr/lib/x86_64-linux-gnu -lfftw3f -lboost_program_options

%.o: %.cpp
	g++ -g -O3 -o $@ -c -I ../target/include -std=gnu++11 $<
