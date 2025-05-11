CXXFLAGS=-Wall -Werror -Wno-unused-variable -std=c++20
CPPFLAGS=-Iinclude -MMD -MP
CXX=g++

LOGGING_SRC=src/logger.cpp
SRCS=$(filter-out $(LOGGING_SRC), $(shell find src -type f -name '*.cpp'))
OBJS=$(patsubst %.cpp, %.o, $(SRCS))
DEPS=$(patsubst %.cpp, %.d, $(SRCS))
INCPATHS=include

DEBUG ?= 0
ifeq ($(DEBUG), 1)
	CXXFLAGS+=-g -fsanitize=address -DDEBUG
else
	CXXFLAGS+=-O3
endif

ENABLE_LOGGING ?= 0
ifeq ($(ENABLE_LOGGING), 1)
	CXXFLAGS+=-DENABLE_LOGGING
	SRCS+=$(LOGGING_SRC)
endif


.PHONY: all clean
all: client

-include $(DEPS)

client: $(OBJS)
	$(CXX) -o $@ $^

clean:
	rm -f $(OBJS) $(patsubst %.cpp, %.o, $(LOGGING_SRC)) $(DEPS) client 
