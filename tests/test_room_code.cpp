#include "network/room_code.hpp"
#include <cassert>
#include <iostream>
#include <vector>

void TestRoomCode() {
  std::cout << "\n--- Testing RoomCode ---\n";
  std::cout << "[TEST] RoomCode Encode/Decode Roundtrip... ";

  std::vector<std::pair<std::string, uint16_t>> testCases = {
      {"127.0.0.1", 8080},
      {"192.168.1.34", 9000},
      {"10.0.0.1", 1234},
      {"172.16.254.1", 65535},
      {"255.255.255.255", 1},
      {"0.0.0.0", 0},
  };

  for (const auto &[ip, port] : testCases) {
    std::string code = RoomCode::EncodeRaw(ip, port);
    assert(!code.empty() && "Code should not be empty");
    assert(code.length() == 9 && "Code should be 8 chars + 1 hyphen = 9 chars");

    PeerAddress decoded;
    bool success = RoomCode::Decode(code, decoded);
    assert(success && "Decode should succeed");
    assert(decoded.ip == ip && "Decoded IP must match original IP");
    assert(decoded.port == port && "Decoded Port must match original Port");
  }
  std::cout << "PASSED ✅\n";

  std::cout << "[TEST] RoomCode Invalid Inputs... ";
  std::string badIpCode = RoomCode::EncodeRaw("999.999.999.999", 8080);
  assert(badIpCode.empty() && "Invalid IP must return empty string");

  PeerAddress addr;
  assert(!RoomCode::Decode("TOO_SHORT", addr) && "Short code must fail");
  assert(!RoomCode::Decode("", addr) && "Empty code must fail");
  std::cout << "PASSED ✅\n";
}
