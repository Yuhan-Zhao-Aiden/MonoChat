#include <string>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <csignal>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "Client.h"
#include "ChatUI.h"

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "Please provide IP and Port" << std::endl;
    return 1;
  }
  signal(SIGPIPE, SIG_IGN); // prevent crash if server closes socket during send()

  SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
  SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr); // self-signed

  int serverFd = socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in server{};
  server.sin_family = AF_INET;
  server.sin_port = htons(std::stoi(argv[2]));
  inet_pton(AF_INET, argv[1], &server.sin_addr);

  if (connect(serverFd, (sockaddr*) &server, sizeof(server)) < 0) {
    std::cerr << "Could not connect to server.\n";
    SSL_CTX_free(ctx);
    close(serverFd);
    return 1;
  }

  SSL* ssl = SSL_new(ctx);
  SSL_set_fd(ssl, serverFd);
  if (SSL_connect(ssl) <= 0) {
    ERR_print_errors_fp(stderr);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(serverFd);
    return 1;
  }

  // --- Username handshake (pre-UI, plain terminal) ---
  char buffer[1024];
  std::string name;
  while (true) {
    std::cout << "What is your name? " << std::flush;
    std::getline(std::cin, name);
    SSL_write(ssl, name.c_str(), static_cast<int>(name.size()));

    int bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1);
    if (bytes <= 0) { std::cerr << "Server disconnected.\n"; SSL_free(ssl); SSL_CTX_free(ctx); return 1; }
    buffer[bytes] = '\0';

    if (strncmp(buffer, "A", 1) == 0) break; // acknowledged
    std::cout << buffer << "\n"; 
  }

  // --- Start ncurses UI ---
  ChatUI ui(name);

  // Network receive thread: pushes messages into the safe queue.
  std::thread receiveThread(receiveLoop, ssl, std::ref(ui.incomingQueue()));

  // UI event loop (blocking, runs on main thread).
  ui.run([&](const std::string& msg) {
    SSL_write(ssl, msg.c_str(), static_cast<int>(msg.size()));
  });

  receiveThread.join();
  SSL_shutdown(ssl);
  SSL_free(ssl);
  SSL_CTX_free(ctx);
  close(serverFd);
}

// Runs on a background thread — only touches the MessageQueue, never ncurses.
void receiveLoop(SSL* ssl, MessageQueue& queue) {
  char buffer[1024];

  while (true) {
    int bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1);
    if (bytes <= 0) {
      queue.push({ "System", "Disconnected from server.", false });
      break;
    }
    buffer[bytes] = '\0';
    // Server messages arrive as "[username]: text" — treat them as remote.
    queue.push({ "", std::string(buffer, bytes), false });
  }
}