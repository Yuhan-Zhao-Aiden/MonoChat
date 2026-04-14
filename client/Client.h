#ifndef CLIENT_H
#define CLIENT_H

#include "ChatUI.h"
#include <openssl/ssl.h>

void receiveLoop(SSL* ssl, MessageQueue& queue);

#endif