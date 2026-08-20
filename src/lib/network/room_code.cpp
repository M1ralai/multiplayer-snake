#include "network/room_code.hpp"
#include <arpa/inet.h>
#include <cstring>

static const char BASE64_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static uint8_t CharTo6Bit(char c) {
  const char *ptr = std::strchr(BASE64_CHARS, c);
  if (!ptr)
    return 0xFF;
  return static_cast<uint8_t>(ptr - BASE64_CHARS);
}

std::string RoomCode::EncodeRaw(const std::string &ip, uint16_t port) {
  uint8_t bytes[6];
  struct in_addr addr;
  if (inet_pton(AF_INET, ip.c_str(), &addr) <= 0) {
    return ""; // Geçersiz IP
  }
  std::memcpy(&bytes[0], &addr.s_addr, 4);

  uint16_t netPort = htons(port);
  std::memcpy(&bytes[4], &netPort, 2);
  uint64_t value = 0;
  for (int i = 0; i < 6; i++) {
    value = (value << 8) | bytes[i];
  }

  std::string result = "";
  for (int shift = 42; shift >= 0; shift -= 6) {
    result += BASE64_CHARS[(value >> shift) & 0x3F];
    if (shift == 24) {
      result += '-'; // Tam ortaya şık bir tire koy (XXXX-YYYY)
    }
  }

  return result;
}

std::string RoomCode::EncodeStruct(const PeerAddress &addr) {
  return RoomCode::EncodeRaw(addr.ip, addr.port);
}

bool RoomCode::Decode(std::string code, PeerAddress &addr) {
  // Eğer 9 karakterse ve ortada tire varsa (XXXX-YYYY), sadece ortadaki tireyi kaldır
  if (code.length() == 9 && code[4] == '-') {
    code.erase(4, 1);
  }

  if (code.length() != 8) {
    return false; // 8 karakterlik geçerli bir kod değil
  }

  uint64_t value = 0;
  for (char c : code) {
    uint8_t sixBit = CharTo6Bit(c);
    if (sixBit == 0xFF) {
      return false; // Base64 tablosunda olmayan geçersiz karakter
    }
    value = (value << 6) | sixBit;
  }

  uint8_t bytes[6];
  // 48 biti tekrar 6 bayta çıkar
  for (int i = 5; i >= 0; i--) {
    bytes[i] = value & 0xFF;
    value >>= 8;
  }

  // 1. IP'ye dönüştür
  char ipBuffer[INET_ADDRSTRLEN];
  if (!inet_ntop(AF_INET, &bytes[0], ipBuffer, INET_ADDRSTRLEN)) {
    return false;
  }
  addr.ip = ipBuffer;

  // 2. Port'a dönüştür
  uint16_t netPort;
  std::memcpy(&netPort, &bytes[4], 2);
  addr.port = ntohs(netPort);

  return true;
}
