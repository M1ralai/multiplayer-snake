#ifndef HCB_MSNAKE_STUN
#define HCB_MSNAKE_STUN

#include "network/connection.hpp"
#include <cstdint>
#include <string>

namespace StunClient {
// Google veya Cloudflare STUN sunucusuna sorarak açık internetteki
// gerçek genel (public) IP ve Port bilgilerini çözer (RFC 5389)
bool GetPublicEndpoint(Connection &conn, std::string &outPublicIp,
                       uint16_t &outPublicPort,
                       const std::string &stunServer = "stun.l.google.com",
                       uint16_t stunPort = 19302);
} // namespace StunClient

#endif // !HCB_MSNAKE_STUN
