
OBJS = scan.o fft.o process.o

scan: $(OBJS)
	g++ -g -o scan $(OBJS) -L ../target/lib -lbladeRF -L /usr/lib/x86_64-linux-gnu -lfftw3f -lboost_program_options

%.o: %.cpp
	g++ -g -O3 -o $@ -c -I ../target/include -std=gnu++11 $<
