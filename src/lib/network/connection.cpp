#include "network/connection.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

Connection::Connection() : socketFd(-1), localPort(0) {}

Connection::~Connection() { Close(); }

bool Connection::Bind(uint16_t port) {
  Close();

  struct addrinfo hints {};
  struct addrinfo *res = nullptr;
  hints.ai_family = AF_INET;     // IPv4
  hints.ai_socktype = SOCK_DGRAM; // UDP
  hints.ai_flags = AI_PASSIVE;    // Tüm yerel ağ arayüzlerini dinle

  std::string portStr = std::to_string(port);
  if (getaddrinfo(nullptr, portStr.c_str(), &hints, &res) != 0) {
    return false;
  }

  socketFd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (socketFd < 0) {
    freeaddrinfo(res);
    return false;
  }

  // Yeniden başlatmalarda port çakışmasını engelle (SO_REUSEADDR)
  int opt = 1;
  setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  if (bind(socketFd, res->ai_addr, res->ai_addrlen) < 0) {
    freeaddrinfo(res);
    Close();
    return false;
  }

  freeaddrinfo(res);
  this->localPort = port;
  return true;
}

bool Connection::SendTo(const std::string &ip, uint16_t port, const void *data,
                        size_t size) {
  if (socketFd < 0) {
    return false;
  }

  struct addrinfo hints {};
  struct addrinfo *res = nullptr;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  std::string portStr = std::to_string(port);
  if (getaddrinfo(ip.c_str(), portStr.c_str(), &hints, &res) != 0) {
    return false;
  }

  ssize_t sent =
      sendto(socketFd, data, size, 0, res->ai_addr, res->ai_addrlen);
  freeaddrinfo(res);

  return sent >= 0;
}

ssize_t Connection::ReceiveFrom(void *buffer, size_t maxSize,
                                std::string &outIp, uint16_t &outPort) {
  if (socketFd < 0) {
    return -1;
  }

  struct sockaddr_in senderAddr {};
  socklen_t addrLen = sizeof(senderAddr);

  ssize_t bytesRead = recvfrom(socketFd, buffer, maxSize, 0,
                               (struct sockaddr *)&senderAddr, &addrLen);

  if (bytesRead > 0) {
    char ipBuffer[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &senderAddr.sin_addr, ipBuffer, sizeof(ipBuffer));
    outIp = ipBuffer;
    outPort = ntohs(senderAddr.sin_port);
  }

  return bytesRead;
}

bool Connection::SetNonBlocking(bool nonBlocking) {
  if (socketFd < 0) {
    return false;
  }

  int flags = fcntl(socketFd, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }

  if (nonBlocking) {
    flags |= O_NONBLOCK;
  } else {
    flags &= ~O_NONBLOCK;
  }

  return fcntl(socketFd, F_SETFL, flags) == 0;
}

void Connection::Close() {
  if (socketFd >= 0) {
    close(socketFd);
    socketFd = -1;
  }
  localPort = 0;
}
