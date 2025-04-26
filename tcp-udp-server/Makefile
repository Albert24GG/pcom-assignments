SERVER_SRC = $(wildcard src/server/*.cpp)
SERVER_OBJ = $(SERVER_SRC:.cpp=.o)
SERVER_BIN = server

SUBSCRIBER_SRC = $(wildcard src/tcp-client/*.cpp)
SUBSCRIBER_OBJ = $(SUBSCRIBER_SRC:.cpp=.o)
SUBSCRIBER_BIN = subscriber

COMMON_SRC = $(wildcard src/common/*.cpp)
COMMON_OBJ = $(COMMON_SRC:.cpp=.o)
COMMON_INC = src/common

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -I$(COMMON_INC)

.PHONY: all
all: $(SERVER_BIN) $(SUBSCRIBER_BIN)

$(SERVER_BIN): $(SERVER_OBJ) $(COMMON_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(SUBSCRIBER_BIN): $(SUBSCRIBER_OBJ) $(COMMON_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

.PHONY: clean
clean:
	rm -f $(SERVER_OBJ) $(SUBSCRIBER_OBJ) $(COMMON_OBJ) $(SERVER_BIN) $(SUBSCRIBER_BIN)


