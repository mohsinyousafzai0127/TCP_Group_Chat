CXX = g++
CXXFLAGS = -std=c++17 -pthread -Wall

all: server client

server: server.cpp protocol.cpp protocol.h
	$(CXX) $(CXXFLAGS) server.cpp protocol.cpp -o server

client: client.cpp protocol.cpp protocol.h
	$(CXX) $(CXXFLAGS) client.cpp protocol.cpp -o client

clean:
	rm -f server client *_seq.txt