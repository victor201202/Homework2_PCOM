CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -std=c++11

SERVER_SRCS = server.cpp
SERVER_TARGET = server

SUBSCRIBER_SRCS = subscriber.cpp
SUBSCRIBER_TARGET = subscriber

.PHONY: all clean

all: $(SERVER_TARGET) $(SUBSCRIBER_TARGET)

$(SERVER_TARGET): $(SERVER_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(SUBSCRIBER_TARGET): $(SUBSCRIBER_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(SERVER_TARGET) $(SUBSCRIBER_TARGET)
