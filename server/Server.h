#ifndef SERVER_H
#define SERVER_H
#include "ClientPool.h"
#include <memory>

int setup(int port);
void client_handler(std::shared_ptr<ClientPool> clientPool, int clientFd);

#endif
