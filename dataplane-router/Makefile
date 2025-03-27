PROJECT=router
C_SOURCES=lib/lib.c
CPP_SOURCES=main.cpp
LIBRARY=nope
INCPATHS=include
LIBPATHS=.
LDFLAGS=
CFLAGS=-c -Wall -Werror -Wno-error=unused-variable
CXXFLAGS=-c -Wall -Werror -Wno-error=unused-variable -std=c++20
CC=gcc
CXX=g++

# Automatic generation of some important lists
OBJECTS=$(C_SOURCES:.c=.o)
OBJECTS+=$(CPP_SOURCES:.cpp=.o)
INCFLAGS=$(foreach TMP,$(INCPATHS),-I$(TMP))
LIBFLAGS=$(foreach TMP,$(LIBPATHS),-L$(TMP))

# Set up the output file names for the different output types
BINARY=$(PROJECT)

all: $(SOURCES) $(BINARY)

$(BINARY): $(OBJECTS)
	$(CXX) $(LIBFLAGS) $(OBJECTS) $(LDFLAGS) -o $@

unit-tests: tests.o
	$(CXX) $(LIBFLAGS) tests.o $(LDFLAGS) -o tests

run-tests: unit-tests
	./tests

%.o: %.c
	$(CC) $(INCFLAGS) $(CFLAGS) -fPIC $< -o $@

%.o: %.cpp
	$(CXX) $(INCFLAGS) $(CXXFLAGS) -fPIC $< -o $@

clean:
	rm -rf $(OBJECTS) router hosts_output router_* tests.o tests

run_router0: all
	./router rtable0.txt rr-0-1 r-0 r-1

run_router1: all
	./router rtable1.txt rr-0-1 r-0 r-1
