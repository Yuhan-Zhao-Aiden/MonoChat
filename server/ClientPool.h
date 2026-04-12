#ifndef CLIENTPOOL_H
#define CLIENTPOOL_H
#include <string>
#include <vector>
#include <mutex>
#include <optional>
#include <memory>

struct ClientInfo {
  int socketFd;
  std::string username;  
};

class ClientPool {
  std::vector<ClientInfo> pool;
  mutable std::mutex clientMutex;

  public:
  ClientPool() = default;
  ~ClientPool() = default;

  ClientPool(const ClientPool&) = delete;
  ClientPool& operator=(const ClientPool&) = delete;

  // client management
  bool addClient(int socketFd, const std::string& username);
  bool removeBySocket(int socketFd);
  bool removeByUsername(const std::string& username);

  // Query
  bool usernameExists(const std::string& username) const;
  bool socketExists(int socketFd) const;
  std::optional<ClientInfo> findBySocket(int socketFd) const;
  std::optional<ClientInfo> findByUsername(const std::string& username) const;

  // list/snapshot
  std::mutex& getMutex();
  std::vector<ClientInfo> getClients() const;
  std::vector<std::string> getUsernames() const;
  std::size_t size() const;
  bool empty() const;
};

class MessageDispatcher {
  public:
  static bool sendToSocket(int socketFd, const std::string& message);
  static void broadcast(const std::shared_ptr<ClientPool> clientPool, const std::string& message, int excludeSocketFd = -1);
};

#endif