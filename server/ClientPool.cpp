#include "ClientPool.h"
#include <sys/socket.h>

bool ClientPool::addClient(int socketFd, const std::string& username) {
  if (usernameExists(username)) return false;
  pool.push_back({ socketFd, username });
  return true;
}

bool ClientPool::removeBySocket(int socketFd) {
  auto num_removed = std::erase_if(pool, [socketFd](const ClientInfo& c) {
    return c.socketFd == socketFd;
  });
  return num_removed > 0;
}

bool ClientPool::removeByUsername(const std::string& username) {
  auto num_removed = std::erase_if(pool, [&username](const ClientInfo& c) {
    return c.username == username;
  });
  return num_removed > 0;
}

bool ClientPool::usernameExists(const std::string& username) const {
  for (const auto& c : pool)
    if (c.username == username) return true;
  return false;
}

bool ClientPool::socketExists(int socketFd) const {
  for (const auto& c : pool)
    if (c.socketFd == socketFd) return true;
  return false;
}

std::optional<ClientInfo> ClientPool::findBySocket(int socketFd) const {
  for (const auto& c : pool)
    if (c.socketFd == socketFd) return c;
  return std::nullopt;
}

std::optional<ClientInfo> ClientPool::findByUsername(const std::string& username) const {
  for (const auto& c : pool)
    if (c.username == username) return c;
  return std::nullopt;
}

std::mutex& ClientPool::getMutex() {
  return clientMutex;
}

std::vector<ClientInfo> ClientPool::getClients() const {
  return pool;
}

std::vector<std::string> ClientPool::getUsernames() const {
  std::vector<std::string> names;
  names.reserve(pool.size());
  for (const auto& c : pool)
    names.push_back(c.username);
  return names;
}

std::size_t ClientPool::size() const {
  return pool.size();
}

bool ClientPool::empty() const {
  return pool.empty();
}


void MessageDispatcher::broadcast(
  const std::shared_ptr<ClientPool> clientPool, 
  const std::string& message, 
  int excludeSocketFd) 
{
  std::lock_guard<std::mutex> lock(clientPool->getMutex());
  for (auto& client : clientPool->getClients()) {
    if (client.socketFd == excludeSocketFd) continue;
    send(client.socketFd, message.c_str(), message.size(), 0);
  }
}