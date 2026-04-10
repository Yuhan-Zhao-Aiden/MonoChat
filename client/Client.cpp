#include <string>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int, char** argv) {
  std::string msg = argv[1];

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in server{};
  server.sin_family = AF_INET;
  server.sin_port = htons(5555);
  inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

  connect(sockfd, (sockaddr*) &server, sizeof(server));

  send(sockfd, msg.c_str(), msg.size(), 0);

  close(sockfd);
}