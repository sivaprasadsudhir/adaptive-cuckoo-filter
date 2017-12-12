CC = g++
AR = ar
PREFIX=/usr/local

# Uncomment one of the following to switch between debug and opt mode
OPT = -O3 -DNDEBUG
#OPT = -g -ggdb

CFLAGS += --std=c++11 -fno-strict-aliasing -Wall -c -I. -I./include -I/usr/include/ -I./src/ $(OPT) 
CFLAGS += -I/usr/local/opt/openssl/include -I ../libcuckoo/install/include
CFLAGS += -I./benchmarks -I/usr/local/include -I/usr/local/lib 

LDFLAGS+= -Wall -lpthread -lssl -lcrypto -L/usr/local/opt/openssl/lib

LIBOBJECTS = \
	./src/hashutil.o \

HEADERS = $(wildcard src/*.h)
ALIB = libcuckoofilter.a

TEST = test

all: $(TEST) old_test cbench cbench_cuckoohash

clean:
	rm -f $(TEST) */*.o

test: example/test.o $(LIBOBJECTS) 
	$(CC) example/test.o $(LIBOBJECTS) $(LDFLAGS) -o $@

old_test: example/old_test.o $(LIBOBJECTS)
	$(CC) example/old_test.o $(LIBOBJECTS) $(LDFLAGS) -o $@

cbench: example/cbench.o $(LIBOBJECTS)
	$(CC) example/cbench.o $(LIBOBJECTS) $(LDFLAGS) -o $@

cbench_cuckoohash: example/cbench_cuckoohash.o $(LIBOBJECTS)
	$(CC) example/cbench_cuckoohash.o $(LIBOBJECTS) $(LDFLAGS) -o $@

benchmark: example/benchmark.o $(LIBOBJECTS) 
	$(CC) example/benchmark.o $(LIBOBJECTS) $(LDFLAGS) -o $@

%.o: %.cc ${HEADERS} Makefile
	$(CC) $(CFLAGS) $< -o $@

$(ALIB): $(LIBOBJECTS)
	$(AR) rcs $@ $(LIBOBJECTS)

.PHONY: install
install: $(ALIB)
	install -D -m 0755 $(HEADERS) -t $(DESTDIR)$(PREFIX)/include/cuckoofilter
	install -D -m 0755 $< -t $(DESTDIR)$(PREFIX)/lib

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/lib/$(ALIB)
	rm -rf $(DESTDIR)$(PREFIX)/include/cuckoofilter
