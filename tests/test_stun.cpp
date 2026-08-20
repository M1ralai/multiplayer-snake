#include "network/connection.hpp"
#include "network/stun.hpp"
#include <cassert>
#include <iostream>

void TestStun() {
  std::cout << "\n--- Testing Google STUN Client (RFC 5389) ---\n";
  std::cout << "[TEST] Querying stun.l.google.com:19302 for Public IP... ";

  Connection conn;
  assert(conn.Bind(9905) && "Failed to bind UDP socket for STUN");
  conn.SetNonBlocking(true);

  std::string publicIp;
  uint16_t publicPort = 0;

  bool success = StunClient::GetPublicEndpoint(conn, publicIp, publicPort,
                                               "stun.l.google.com", 19302);
  if (success) {
    std::cout << "SUCCESS! ✅\n";
    std::cout << "       -> Dış IP (Public IP): " << publicIp << "\n";
    std::cout << "       -> Dış Port (Mapped Port): " << publicPort << "\n";
  } else {
    // İnternet veya DNS yoksa skip
    std::cout << "SKIPPED ⚠️ (Internet connection or UDP port blocked)\n";
  }

  conn.Close();
}
