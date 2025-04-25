SERVER_SRC = $(shell echo src/server/{main,server,subscribers_registry,tcp_proto,tcp_utils,token_pattern,udp_proto}.cpp)
SERVER_OBJ = $(SERVER_SRC:.cpp=.o)
SERVER_INC = 
SERVER_BIN = server

SUBSCRIBER_SRC = $(shell echo src/tcp-client/{main,client}.cpp src/server/{tcp_proto,tcp_utils,token_pattern}.cpp)
SUBSCRIBER_OBJ = $(SUBSCRIBER_SRC:.cpp=.o)
SUBSCRIBER_INC =
SUBSCRIBER_BIN = subscriber

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3

.PHONY: all
all: $(SERVER_BIN) $(SUBSCRIBER_BIN)

$(SERVER_BIN): $(SERVER_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(SUBSCRIBER_BIN): $(SUBSCRIBER_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

.PHONY: clean
clean:
	rm -f $(SERVER_OBJ) $(SUBSCRIBER_OBJ) $(SERVER_BIN) $(SUBSCRIBER_BIN)


