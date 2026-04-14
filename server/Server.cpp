#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <cstring>
#include <cerrno>
#include <csignal>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "Server.h"

// Accessible to the signal handler — must be file-scope.
static volatile sig_atomic_t g_running = 1;
static int g_server_fd = -1;
static SSL_CTX* g_ssl_ctx = nullptr;

static void handle_shutdown(int) {
  g_running = 0;
  // Unblock the blocking accept() on the main thread.
  if (g_server_fd != -1)
    shutdown(g_server_fd, SHUT_RDWR);
}

SSL_CTX* setup_ssl() {
  SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
  if (!ctx) { ERR_print_errors_fp(stderr); exit(1); }

  const char* cert = getenv("SERVER_CERT");
  if (!cert) cert = "/app/cert.pem";
  const char* key  = getenv("SERVER_KEY");
  if (!key)  key  = "/app/key.pem";

  if (SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_PEM) <= 0) {
    ERR_print_errors_fp(stderr); SSL_CTX_free(ctx); exit(1);
  }
  if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) <= 0) {
    ERR_print_errors_fp(stderr); SSL_CTX_free(ctx); exit(1);
  }
  if (!SSL_CTX_check_private_key(ctx)) {
    std::cerr << "Certificate and private key do not match\n";
    SSL_CTX_free(ctx); exit(1);
  }
  return ctx;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: server <port>\n";
    return 1;
  }
  int port = std::stoi(argv[1]);
  std::cout << "Server started on port " << port << " (Ctrl-C to stop)\n";

  signal(SIGPIPE, SIG_IGN);
  signal(SIGINT,  handle_shutdown);
  signal(SIGTERM, handle_shutdown);

  g_server_fd = setup(port);
  g_ssl_ctx = setup_ssl();
  auto pool = std::make_shared<ClientPool>();

  while (g_running) {
    int client_fd = accept(g_server_fd, nullptr, nullptr);
    if (client_fd < 0) break;

    SSL* ssl = SSL_new(g_ssl_ctx);
    SSL_set_fd(ssl, client_fd);
    if (SSL_accept(ssl) <= 0) {
      ERR_print_errors_fp(stderr);
      SSL_free(ssl);
      close(client_fd);
      continue;
    }
    std::thread(client_handler, pool, client_fd, ssl).detach();
  }

  MessageDispatcher::broadcast(pool, "[System]: Server is shutting down.");
  close(g_server_fd);
  g_server_fd = -1;
  SSL_CTX_free(g_ssl_ctx);
  g_ssl_ctx = nullptr;
  std::cout << "\nServer stopped.\n";
}

void client_handler(std::shared_ptr<ClientPool> clientPool, int clientFd, SSL* ssl) {
  // 1. Get username
  char buffer[1024];
  std::string name;
  while (true) {
    int bytes = SSL_read(ssl, buffer, sizeof(buffer));
    if (bytes <= 0) {
      SSL_free(ssl);
      close(clientFd); // client disconnected before sending a username
      return;
    }

    // Hold the lock only for the check-and-insert
    std::string proposed(buffer, bytes);
    bool taken;
    {
      std::lock_guard<std::mutex> lock(clientPool->getMutex());
      taken = clientPool->usernameExists(proposed);
      if (!taken)
        clientPool->addClient(clientFd, proposed, ssl);
    }

    if (taken) {
      std::string msg = "Username already taken!";
      SSL_write(ssl, msg.c_str(), static_cast<int>(msg.size()));
      continue;
    }

    SSL_write(ssl, "A", 1);
    name = proposed;
    break;
  }

  // 3. Broadcast join message
  MessageDispatcher::broadcast(clientPool, "[System]: " + name + " has joined the chat!");

  // 4. Loop to handle messages (basic for now)
  while (true) {
    char buffer[1024];
    int bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1);
    if (bytes <= 0) break; // client disconnected or error
    buffer[bytes] = '\0';
    if (strncmp(buffer, "exit", 4) == 0) {
      std::cout << name << " has quit." << std::endl;
      break;
    }
    std::string msg(buffer, bytes);
    MessageDispatcher::broadcast(clientPool, "[" + name + "]: " + msg, clientFd);
  }

  // 5. Remove from client pool (ssl pointer no longer accessible via pool after this)
  {
    std::lock_guard<std::mutex> lock(clientPool->getMutex());
    clientPool->removeByUsername(name);
  }

  // 6. Broadcast quit message
  MessageDispatcher::broadcast(clientPool, "[System]: " + name + " has left the chat!");

  // 7. Clean up TLS and socket for this client
  SSL_shutdown(ssl);
  SSL_free(ssl);
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
  listen(server_fd, SOMAXCONN);
  return server_fd;
}