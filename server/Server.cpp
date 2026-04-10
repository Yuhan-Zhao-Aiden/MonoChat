#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "Server.h"

int main(int, char** argv) {
  int port = std::stoi(argv[1]);
  std::cout << "Hello World! port = " << port << std::endl;

  int server_fd = setup(port);
  int client_fd = accept(server_fd, nullptr, nullptr); // blocking

  char buffer[1024];
  int bytes = recv(client_fd, buffer, sizeof(buffer), 0);

  if (bytes > 0) {
    std::string msg(buffer, bytes);
    std::cout << msg << std::endl;
  }

  close(client_fd);
  close(server_fd);
}

int setup(int port) {
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  bind(server_fd, (sockaddr*) &addr, sizeof(addr));
  listen(server_fd, 5);
  return server_fd;
}