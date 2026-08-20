#ifndef HCB_MSNAKE_PACKET
#define HCB_MSNAKE_PACKET

#include <cstdint>

#define PROTOCOL_VERSION 1

enum class PacketType : uint8_t {
  JOIN_REQUEST = 1,
  JOIN_ACCEPT = 2,
  JOIN_REFUSE = 3,
  GAME_START = 4,
  INPUT = 5,
  PING = 6,
  PONG = 7,
  DISCONNECT = 8
};

enum class RefuseReason : uint8_t {
  ROOM_FULL = 1,
  ALREADY_IN_GAME = 2,
  VERSION_MISMATCH = 3
};

#pragma pack(push, 1)

// Genel başlık
struct PacketHeader {
  PacketType type;
};

// 1. Odaya Katılma İsteği
struct JoinRequestPacket {
  PacketType type{PacketType::JOIN_REQUEST};
  uint8_t protocolVersion{PROTOCOL_VERSION};
  char playerName[16]{0};
};

// 2. Katılma Kabul Paketi (Host -> Joiner)
struct JoinAcceptPacket {
  PacketType type{PacketType::JOIN_ACCEPT};
  uint8_t assignedPlayerId; // 1 = Joiner (0 = Host)
  uint32_t randomSeed;      // Elma yerleşimi ve determinizm için ortak tohum
};

// 3. Katılma Red Paketi (Host -> Joiner)
struct JoinRefusePacket {
  PacketType type{PacketType::JOIN_REFUSE};
  RefuseReason reason;
};

// 4. Oyunu Başlatma Paketi (Host -> Joiner)
struct GameStartPacket {
  PacketType type{PacketType::GAME_START};
  uint32_t startTick;
};

// 5. Oyuncu Girdisi (Her iki taraf birbirine gönderir)
struct InputPacket {
  PacketType type{PacketType::INPUT};
  uint8_t playerId; // 0 veya 1
  uint32_t tick;    // Hangi tick için
  int8_t dirX;      // Point::x (-1, 0, 1)
  int8_t dirY;      // Point::y (-1, 0, 1)
};

// 6. Ping / Pong
struct PingPongPacket {
  PacketType type; // PING veya PONG
  uint64_t timestamp;
};

// 7. Ayrılma / Çıkış
struct DisconnectPacket {
  PacketType type{PacketType::DISCONNECT};
  uint8_t playerId;
};

#pragma pack(pop)

#endif // !HCB_MSNAKE_PACKET
