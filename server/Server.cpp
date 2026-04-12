#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <cstring>
#include <cerrno>
#include <csignal>
#include "Server.h"

// Accessible to the signal handler — must be file-scope.
static volatile sig_atomic_t g_running = 1;
static int g_server_fd = -1;

static void handle_shutdown(int) {
  g_running = 0;
  // Unblock the blocking accept() on the main thread.
  if (g_server_fd != -1)
    shutdown(g_server_fd, SHUT_RDWR);
}

int main(int argc, char** argv) {
  if (argc < 2) return 1;
  int port = std::stoi(argv[1]);
  std::cout << "Server started on port " << port << " (Ctrl-C to stop)\n";

  signal(SIGPIPE, SIG_IGN);
  signal(SIGINT,  handle_shutdown);
  signal(SIGTERM, handle_shutdown);

  g_server_fd = setup(port);
  auto pool = std::make_shared<ClientPool>();

  while (g_running) {
    int client_fd = accept(g_server_fd, nullptr, nullptr);
    if (client_fd < 0) break;
    std::thread(client_handler, pool, client_fd).detach();
  }

  MessageDispatcher::broadcast(pool, "[System]: Server is shutting down.");
  close(g_server_fd);
  g_server_fd = -1;
  std::cout << "\nServer stopped.\n";
}

void client_handler(std::shared_ptr<ClientPool> clientPool, int clientFd) {
  // 1. Get username
  char buffer[1024];
  std::string name;
  while (true) {
    int bytes = recv(clientFd, buffer, sizeof(buffer), 0);
    if (bytes <= 0) {
      close(clientFd); // client disconnected before sending a username
      return;
    }
    if (clientPool->usernameExists(std::string(buffer, bytes))) {
      std::string msg = "Username already exist!";
      send(clientFd, msg.c_str(), msg.size(), 0);
      continue;
    }

    // 2. Add to shared list (with mutex)
    send(clientFd, "A", 1, 0); // Acknowledge
    name = std::string(buffer, bytes);
    std::lock_guard<std::mutex> lock(clientPool->getMutex());
    clientPool->addClient(clientFd, name);
    break;
  }

  // 3. Broadcast join message
  MessageDispatcher::broadcast(clientPool, "[System]: " + name + " has joined the chat!");

  // 4. Loop to handle messages (basic for now)
  while (true) {
    char buffer[1024];
    int bytes = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) break; // client disconnected or error
    buffer[bytes] = '\0';
    if (strncmp(buffer, "exit", 4) == 0) {
      std::cout << name << " has quit." << std::endl;
      break;
    }
    std::string msg(buffer, bytes);
    MessageDispatcher::broadcast(clientPool, "[" + name + "]: " + msg, clientFd);
  }

  // 5. Remove from client pool
  {
    std::lock_guard<std::mutex> lock(clientPool->getMutex());
    clientPool->removeByUsername(name);
  }

  // 6. Broadcast quit message
  MessageDispatcher::broadcast(clientPool, "[System]: " + name + " has left the chat!");

  close(clientFd);
}

int setup(int port) {
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);

  // Allow reuse of the port immediately after the server restarts,
  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(server_fd, (sockaddr*) &addr, sizeof(addr)) < 0) {
    std::cerr << "bind() failed on port " << port << ": " << strerror(errno) << "\n";
    close(server_fd);
    exit(1);
  }
  listen(server_fd, 5);
  return server_fd;
}