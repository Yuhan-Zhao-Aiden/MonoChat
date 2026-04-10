CC = g++
CFLAGS = -Wall -Wextra -pedantic -g -std=c++20
TARGET = server.out client.out

all: $(TARGET)

server.out: server.o
	$(CC) $(CFLAGS) $^ -o $@

client.out: client.o
	$(CC) $(CFLAGS) $^ -o $@

server.o: server/Server.cpp
	$(CC) $(CFLAGS) -c $^ -o $@

client.o: client/Client.cpp
	$(CC) $(CFLAGS) -c $^ -o $@

clean:
	rm -f *.o $(TARGET)