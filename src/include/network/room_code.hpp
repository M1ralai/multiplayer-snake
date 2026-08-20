#ifndef HCB_MSNAKE_ROOM_CODE
#define HCB_MSNAKE_ROOM_CODE

#include <cstdint>
#include <string>

struct PeerAddress {
  std::string ip;
  uint16_t port;
};

namespace RoomCode {
std::string EncodeRaw(const std::string &ip, uint16_t port);
std::string EncodeStruct(const PeerAddress &addr);

bool Decode(std::string str, PeerAddress &addr);
} // namespace RoomCode

#endif // !HCB_MSNAKE_ROOM_CODE
