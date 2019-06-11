CXX = g++
CXXFLAGS = -std=c++14 -Wall `pkg-config --cflags libndn-cxx` -I/usr/local/include -g
LIBS = `pkg-config --libs libndn-cxx` -L/usr/local/lib -lleveldb
DESTDIR ?= /usr/local
SOURCE_OBJS = ledger-record.o backend.o peer.o dledger-peer.o
DB_SOUCE_OBJS = backend-test.o ledger-record.o backend.o
PROGRAMS = dledger-peer

all: $(PROGRAMS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $< $(LIBS)

dledger-peer: $(SOURCE_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(SOURCE_OBJS) $(LIBS)

backend-test: $(DB_SOUCE_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(DB_SOUCE_OBJS) $(LIBS)

clean:
	rm -f $(PROGRAMS) *.o

install: all
	cp $(PROGRAMS) $(DESTDIR)/bin/

uninstall:
	cd $(DESTDIR)/bin && rm -f $(PROGRAMS)
