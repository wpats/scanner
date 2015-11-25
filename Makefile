
OBJS := scan.o fft.o process.o bladerfSource.o b210Source.o airspySource.o
HEADERS := scan.h signalSource.h process.h fft.h bladerfSource.h b210Source.h airspySource.h
LIBS = -lfftw3f -lboost_program_options -lboost_thread -lboost_system 

scan: $(OBJS) Makefile
	g++ -g -o scan $(OBJS) -L ../target/lib -lbladeRF -luhd -lairspy -L /usr/lib/x86_64-linux-gnu $(LIBS)

clean:
	rm *.o

%.o: %.cpp $(HEADERS) Makefile 
	g++ -g -o $@ -c -I ../target/include -std=gnu++11 $<
