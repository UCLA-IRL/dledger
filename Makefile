CXX = g++
CXXFLAGS = -std=c++14 -g
LIBRFLAGS = `pkg-config --cflags --libs libndn-cxx` -lleveldb
DESTDIR ?= /usr/local
SOURCE_OBJS = ledger-record.o backend.o peer.o dledger-peer.o
DB_SOUCE_OBJS = backend-test.o ledger-record.o backend.o
PROGRAMS = dledger-peer

all: $(PROGRAMS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $< $(LIBRFLAGS)

dledger-peer: $(SOURCE_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(SOURCE_OBJS) $(LIBRFLAGS)

backend-test: $(DB_SOUCE_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(DB_SOUCE_OBJS) $(LIBRFLAGS)

clean:
	rm -f $(PROGRAMS) backend-test *.o

install: all
	cp $(PROGRAMS) $(DESTDIR)/bin/

uninstall:
	cd $(DESTDIR)/bin && rm -f $(PROGRAMS)
