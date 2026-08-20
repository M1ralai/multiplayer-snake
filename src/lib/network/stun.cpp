#include "network/stun.hpp"
#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <random>
#include <thread>

#define STUN_MAGIC_COOKIE 0x2112A442

#pragma pack(push, 1)
struct StunHeader {
  uint16_t msgType;
  uint16_t msgLength;
  uint32_t magicCookie;
  uint8_t transactionId[12];
};

struct StunAttrHeader {
  uint16_t type;
  uint16_t length;
};
#pragma pack(pop)

bool StunClient::GetPublicEndpoint(Connection &conn, std::string &outPublicIp,
                                   uint16_t &outPublicPort,
                                   const std::string &stunServer,
                                   uint16_t stunPort) {
  if (!conn.IsOpen()) {
    return false;
  }

  // 1. 20 Baytlık STUN Binding Request Hazırla (RFC 5389)
  StunHeader req{};
  req.msgType = htons(0x0001);   // Binding Request
  req.msgLength = htons(0x0000); // 0 attr
  req.magicCookie = htonl(STUN_MAGIC_COOKIE);

  // Rastgele 12 bayt Transaction ID
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint32_t> dis;
  uint32_t *tPtr = reinterpret_cast<uint32_t *>(req.transactionId);
  tPtr[0] = dis(gen);
  tPtr[1] = dis(gen);
  tPtr[2] = dis(gen);

  // 2. STUN Sunucusuna Gönder
  bool sent = conn.SendTo(stunServer, stunPort, &req, sizeof(req));
  if (!sent) {
    return false;
  }

  // 3. Cevabı Dinle (Timeout ile)
  uint8_t buffer[512];
  std::string senderIp;
  uint16_t senderPort = 0;

  bool received = false;
  ssize_t bytesRead = 0;

  for (int i = 0; i < 20; i++) { // Max 1 saniye bekle (20 * 50ms)
    bytesRead =
        conn.ReceiveFrom(buffer, sizeof(buffer), senderIp, senderPort);
    if (bytesRead >= static_cast<ssize_t>(sizeof(StunHeader))) {
      auto *res = reinterpret_cast<StunHeader *>(buffer);
      uint16_t resType = ntohs(res->msgType);

      // 0x0101 = Binding Response
      if (resType == 0x0101 &&
          std::memcmp(res->transactionId, req.transactionId, 12) == 0) {
        received = true;
        break;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  if (!received) {
    return false;
  }

  // 4. STUN Niteliklerini (Attributes) Ayrıştır
  size_t offset = sizeof(StunHeader);
  size_t totalLen = static_cast<size_t>(bytesRead);

  while (offset + sizeof(StunAttrHeader) <= totalLen) {
    auto *attr = reinterpret_cast<StunAttrHeader *>(&buffer[offset]);
    uint16_t attrType = ntohs(attr->type);
    uint16_t attrLen = ntohs(attr->length);

    offset += sizeof(StunAttrHeader);
    if (offset + attrLen > totalLen)
      break;

    // 0x0020: XOR-MAPPED-ADDRESS (RFC 5389 standardı)
    if (attrType == 0x0020 && attrLen >= 8) {
      uint8_t family = buffer[offset + 1];
      if (family == 0x01) { // IPv4
        uint16_t rawPort;
        std::memcpy(&rawPort, &buffer[offset + 2], 2);
        outPublicPort = ntohs(rawPort) ^ (STUN_MAGIC_COOKIE >> 16);

        uint32_t rawIp;
        std::memcpy(&rawIp, &buffer[offset + 4], 4);
        uint32_t xorIp = ntohl(rawIp) ^ STUN_MAGIC_COOKIE;

        struct in_addr inAddr;
        inAddr.s_addr = htonl(xorIp);
        char ipBuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &inAddr, ipBuf, INET_ADDRSTRLEN);
        outPublicIp = ipBuf;

        return true;
      }
    }
    // 0x0001: MAPPED-ADDRESS (Eski RFC 3489 fallback)
    else if (attrType == 0x0001 && attrLen >= 8) {
      uint8_t family = buffer[offset + 1];
      if (family == 0x01) { // IPv4
        uint16_t rawPort;
        std::memcpy(&rawPort, &buffer[offset + 2], 2);
        outPublicPort = ntohs(rawPort);

        char ipBuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &buffer[offset + 4], ipBuf, INET_ADDRSTRLEN);
        outPublicIp = ipBuf;

        return true;
      }
    }

    // STUN nitelikleri 4 bayta hizalıdır
    offset += (attrLen + 3) & ~3;
  }

  return false;
}
