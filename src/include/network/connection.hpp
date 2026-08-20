#ifndef HCB_MSNAKE_CONNECTION
#define HCB_MSNAKE_CONNECTION

#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/types.h>

class Connection {
private:
  int socketFd{-1};
  uint16_t localPort{0};

public:
  Connection();
  ~Connection();

  // Socket'i belirtilen yerel port'a bağlar (Bind)
  bool Bind(uint16_t port);

  // Belirtilen IP ve Port'a UDP paketi gönderir
  bool SendTo(const std::string &ip, uint16_t port, const void *data,
              size_t size);

  // Gelen UDP paketini okur; gönderenin IP ve Port bilgilerini doldurur
  // Blocking veya Non-blocking modda çalışabilir
  ssize_t ReceiveFrom(void *buffer, size_t maxSize, std::string &outIp,
                      uint16_t &outPort);

  // Socket'in I/O modunu non-blocking (beklemesiz) yapar
  bool SetNonBlocking(bool nonBlocking);

  // Soketi kapatır
  void Close();

  bool IsOpen() const { return socketFd != -1; }
  uint16_t GetLocalPort() const { return localPort; }
};

#endif // !HCB_MSNAKE_CONNECTION
