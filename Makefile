CFLAGS = -std=c++11 -Werror -pedantic

all: ipk-mtrip

ipk-mtrip: ipk-mtrip.o
	g++ $(CFLAGS) $^ -o ipk-mtrip

ipk-mtrip.o: ipk-mtrip.cpp
	g++ $(CFLAGS) ipk-mtrip.cpp -c

clean:
	rm -rf ipk-mtrip ipk-mtrip.o

zip:
	zip xcurda02.zip Makefile dokumentace.pdf readme ipk-mtrip.cpp
