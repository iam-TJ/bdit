OUTPUT=bdit
SRC=block_device_integrity_tester.cpp
CPP=g++
CXXFLAGS=-std=c++11 -g


all:
	$(CPP) $(CXXFLAGS) -o $(OUTPUT) $(SRC)


clean:
	rm -f $(OUTPUT) *.o

.PHONY: all clean

