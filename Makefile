
OBJS := scan.o fft.o process.o signalSource.o sampleBuffer.o \
	arguments.o processInterface.o utility.o frequencyTable.o \
	bladerfSource.o b210Source.o airspySource.o sdrplaySource.o \
	hackRFSource.o rtlSource.o

HEADERS := scan.h signalSource.h process.h fft.h messageQueue.h \
	bladerfSource.h b210Source.h airspySource.h hackRFSource.h

LIBS = -lfftw3f -lboost_program_options -lboost_system -lgnuradio-fft\
	 -lgnuradio-filter -lvolk -lpthread
HARDWARE_LIBS = -lbladeRF -luhd -lairspy -lmirsdrapi-rsp -lhackrf -lrtlsdr

scan: $(OBJS) Makefile
	g++ -g -o scan $(OBJS) -L ../target/lib $(HARDWARE_LIBS) -L /usr/lib/x86_64-linux-gnu $(LIBS)

clean:
	rm *.o

sampleBuffer.o: sampleBuffer.cpp buffer.cpp sampleBuffer.h

%.o: %.cpp $(HEADERS)  Makefile 
	g++ -g -O3 -D INCLUDE_B210 -o $@ -c -I ../target/include -std=gnu++11 $<

