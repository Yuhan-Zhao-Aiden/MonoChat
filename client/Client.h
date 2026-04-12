#ifndef CLIENT_H
#define CLIENT_H

#include "ChatUI.h"

void receiveLoop(int socketFd, MessageQueue& queue);

#endif