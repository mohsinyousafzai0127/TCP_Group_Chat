CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread

all: server client

server: server.cpp protocol.cpp
	$(CXX) $(CXXFLAGS) server.cpp protocol.cpp -o server

client: client.cpp protocol.cpp
	$(CXX) $(CXXFLAGS) client.cpp protocol.cpp -o client

clean:
	rm -f server client *.o
