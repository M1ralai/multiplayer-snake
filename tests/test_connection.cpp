#include "network/connection.hpp"
#include <cassert>
#include <cstring>
#include <iostream>

void TestConnection() {
  std::cout << "\n--- Testing UDP Connection ---\n";
  std::cout << "[TEST] Connection UDP Send/Receive (Loopback)... ";

  Connection peerA;
  Connection peerB;

  assert(peerA.Bind(9901) && "PeerA should bind to port 9901");
  assert(peerB.Bind(9902) && "PeerB should bind to port 9902");

  // PeerA -> PeerB'ye mesaj gönderir
  const char *msg = "PING_FROM_A";
  bool sent = peerA.SendTo("127.0.0.1", 9902, msg, std::strlen(msg) + 1);
  assert(sent && "PeerA should successfully send packet");

  // PeerB mesajı okur
  char buffer[128] = {0};
  std::string senderIp;
  uint16_t senderPort = 0;

  ssize_t bytesRead =
      peerB.ReceiveFrom(buffer, sizeof(buffer), senderIp, senderPort);
  assert(bytesRead > 0 && "PeerB should receive bytes");
  assert(std::strcmp(buffer, "PING_FROM_A") == 0 &&
         "Payload should match sent message");
  assert(senderPort == 9901 && "Sender port should be 9901");

  // PeerB -> PeerA'ya cevap verir (PONG)
  const char *reply = "PONG_FROM_B";
  sent = peerB.SendTo(senderIp, senderPort, reply, std::strlen(reply) + 1);
  assert(sent && "PeerB should reply to PeerA");

  // PeerA cevabı okur
  char replyBuffer[128] = {0};
  bytesRead = peerA.ReceiveFrom(replyBuffer, sizeof(replyBuffer), senderIp,
                                senderPort);
  assert(bytesRead > 0 && "PeerA should receive reply");
  assert(std::strcmp(replyBuffer, "PONG_FROM_B") == 0 &&
         "Reply payload should match");
  assert(senderPort == 9902 && "Sender port should be 9902");

  peerA.Close();
  peerB.Close();
  std::cout << "PASSED ✅\n";

  std::cout << "[TEST] Connection Non-blocking mode... ";
  Connection peer;
  assert(peer.Bind(9903) && "Peer should bind to port 9903");
  assert(peer.SetNonBlocking(true) && "Should enable non-blocking");

  char nonBlockBuf[64];
  ssize_t result =
      peer.ReceiveFrom(nonBlockBuf, sizeof(nonBlockBuf), senderIp, senderPort);
  assert(result < 0 && "Receive on empty non-blocking socket should return -1");

  peer.Close();
  std::cout << "PASSED ✅\n";
}
