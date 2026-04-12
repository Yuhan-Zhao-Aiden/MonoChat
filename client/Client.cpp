#include <string>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <csignal>
#include "Client.h"
#include "ChatUI.h"

int main(int, char**) {
  signal(SIGPIPE, SIG_IGN); // prevent crash if server closes socket during send()

  int serverFd = socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in server{};
  server.sin_family = AF_INET;
  server.sin_port = htons(5555);
  inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

  if (connect(serverFd, (sockaddr*) &server, sizeof(server)) < 0) {
    std::cerr << "Could not connect to server.\n";
    close(serverFd);
    return 1;
  }

  // --- Username handshake (pre-UI, plain terminal) ---
  char buffer[1024];
  std::string name;
  while (true) {
    std::cout << "What is your name? " << std::flush;
    std::getline(std::cin, name);
    send(serverFd, name.c_str(), name.size(), 0);

    int bytes = recv(serverFd, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) { std::cerr << "Server disconnected.\n"; return 1; }
    buffer[bytes] = '\0';

    if (strncmp(buffer, "A", 1) == 0) break; // acknowledged
    std::cout << buffer << "\n";              // "Username already exists!" etc.
  }

  // --- Start ncurses UI ---
  ChatUI ui(name);

  // Network receive thread: pushes messages into the safe queue.
  std::thread receiveThread(receiveLoop, serverFd, std::ref(ui.incomingQueue()));

  // UI event loop (blocking, runs on main thread).
  ui.run([&](const std::string& msg) {
    send(serverFd, msg.c_str(), msg.size(), 0);
  });

  receiveThread.join();
  close(serverFd);
}

// Runs on a background thread — only touches the MessageQueue, never ncurses.
void receiveLoop(int socketFd, MessageQueue& queue) {
  char buffer[1024];

  while (true) {
    int bytes = recv(socketFd, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
      queue.push({ "System", "Disconnected from server.", false });
      break;
    }
    buffer[bytes] = '\0';
    // Server messages arrive as "[username]: text" — treat them as remote.
    queue.push({ "", std::string(buffer, bytes), false });
  }
}