# Uncomment one of the following to switch between debug and opt mode
OPT = -O3 -DNDEBUG
#OPT = -g -ggdb

CXXFLAGS += -fno-strict-aliasing -Wall -std=c++11 -I. -I../src/ $(OPT) -march=core-avx2 
CXXFLAGS += -I/usr/local/opt/openssl/include -I ../../libcuckoo/install/include

LDFLAGS+= -Wall -lpthread -lssl -lcrypto -L/usr/local/opt/openssl/lib

HEADERS = $(wildcard ../src/*.h) *.h

SRC = ../src/hashutil.cc

.PHONY: all

BINS = bulk-insert-and-query.exe

all: $(BINS)

clean:
	/bin/rm -f $(BINS)

%.exe: %.cc ${HEADERS} ${SRC} Makefile
	$(CXX) $(CXXFLAGS) $< -o $@ $(SRC) $(LDFLAGS)
