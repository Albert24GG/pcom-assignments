PROJECT=router
SOURCES=main.cpp lib/lib.c logger.cpp router.cpp routing-table.cpp arp-table.cpp
LIBRARY=nope
INCPATHS=include
LIBPATHS=.
LDFLAGS=
CFLAGS=-c -Wall -Werror -Wno-error=unused-variable -Wno-error=format-overflow
CXXFLAGS=-c -Wall -Werror -Wno-error=unused-variable -std=c++17
CC=gcc
CXX=g++

DEBUG ?= 0
ifeq ($(DEBUG), 1)
	CFLAGS += -g -fsanitize=address -DDEBUG
	CXXFLAGS += -g -fsanitize=address -DDEBUG
else
	CFLAGS += -O3
	CXXFLAGS += -O3
endif


# Automatic generation of some important lists
OBJECTS=$(patsubst %.c, %.o, $(patsubst %.cpp, %.o, $(SOURCES)))
INCFLAGS=$(foreach TMP,$(INCPATHS),-I$(TMP))
LIBFLAGS=$(foreach TMP,$(LIBPATHS),-L$(TMP))

# Set up the output file names for the different output types
BINARY=$(PROJECT)

all: $(SOURCES) $(BINARY)

$(BINARY): $(OBJECTS)
	$(CXX) $(LIBFLAGS) $(OBJECTS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(INCFLAGS) $(CFLAGS) -fPIC $< -o $@

%.o: %.cpp
	$(CXX) $(INCFLAGS) $(CXXFLAGS) -fPIC $< -o $@

clean:
	rm -rf $(OBJECTS) router hosts_output router_*

run_router0: all
	./router rtable0.txt rr-0-1 r-0 r-1

run_router1: all
	./router rtable1.txt rr-0-1 r-0 r-1
