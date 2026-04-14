#ifndef SERVER_H
#define SERVER_H
#include "ClientPool.h"
#include <memory>
#include <openssl/ssl.h>

int setup(int port);
SSL_CTX* setup_ssl();
void client_handler(std::shared_ptr<ClientPool> clientPool, int clientFd, SSL* ssl);

#endif
