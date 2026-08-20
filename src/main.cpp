#include "game/game.hpp"
#include "network/connection.hpp"
#include "network/packet.hpp"
#include "network/room_code.hpp"
#include "network/stun.hpp"
#include "network/upnp.hpp"
#include <arpa/inet.h>
#include <chrono>
#include <ifaddrs.h>
#include <iostream>
#include <netinet/in.h>
#include <random>
#include <thread>

// Yerel makinenin Wi-Fi / Ethernet IPv4 adresini bulur
static std::string GetLocalIP() {
  struct ifaddrs *ifaddr = nullptr;
  if (getifaddrs(&ifaddr) == -1) {
    return "127.0.0.1";
  }

  std::string localIp = "127.0.0.1";
  for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
      continue;

    char host[INET_ADDRSTRLEN];
    auto *sa = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
    inet_ntop(AF_INET, &(sa->sin_addr), host, INET_ADDRSTRLEN);
    std::string ipStr(host);

    if (ipStr != "127.0.0.1") {
      localIp = ipStr;
      break;
    }
  }

  freeifaddrs(ifaddr);
  return localIp;
}

void HostLobby() {
  std::string localIp = GetLocalIP();

  std::cout << "\nBaglanti Turu Secin:\n";
  std::cout << "[1] Yerel Ag / LAN (Ayni Wi-Fi / Localhost)\n";
  std::cout << "[2] Internet / Dunya Geneli (UPnP Otomatik Port / STUN)\n";
  std::cout << "Secim [1-2]: ";

  int netType = 1;
  std::cin >> netType;

  uint16_t localPort = 8080;
  Connection listener;
  while (!listener.Bind(localPort) && localPort < 8090) {
    localPort++;
  }
  listener.SetNonBlocking(true);

  std::string roomIp = localIp;
  uint16_t roomPort = localPort;

  if (netType == 2) {
    std::cout << "\n[*] Modeme UPnP port acma istegi gonderiliyor...\n";
    std::string upnpIp;
    if (UPnP::ForwardPort(localPort, localIp, upnpIp) && !upnpIp.empty()) {
      roomIp = upnpIp;
      roomPort = localPort;
      std::cout << "[+] BASARILI! Modemden UDP " << localPort
                << " portu otomatik acildi! (WAN IP: " << upnpIp << ")\n";
    } else {
      std::cout << "[-] Modem UPnP'ye izin vermedi, Google STUN deneniyor...\n";
      std::string pubIp;
      uint16_t pubPort;
      if (StunClient::GetPublicEndpoint(listener, pubIp, pubPort)) {
        roomIp = pubIp;
        roomPort = localPort; // Modemden yönlendirilen port 8080
        std::cout << "[+] Dış IP: " << pubIp << " | Yönlendirilen Port: " << localPort
                  << "\n";
      }
    }
  }

  std::string roomCode = RoomCode::EncodeRaw(roomIp, roomPort);

  std::cout << "\n========================================\n";
  std::cout << "        MSNAKE ODA KURULDU (HOST)       \n";
  std::cout << "========================================\n";
  std::cout << "IP: " << roomIp << " | Port: " << roomPort << "\n";
  std::cout << "Arkadasina verecegin ODA KODU: \033[1;32m" << roomCode
            << "\033[0m\n";
  std::cout << "Oyuncu bekleniyor... (Cikmak icin Ctrl+C)\n";

  char buffer[256];
  std::string peerIp;
  uint16_t peerPort = 0;
  auto lastKeepAlive = std::chrono::steady_clock::now();

  // Katılma isteği bekle
  while (true) {
    // STUN NAT portunu canlı tutmak için her 2 saniyede bir ping at
    if (netType == 2) {
      auto now = std::chrono::steady_clock::now();
      if (std::chrono::duration_cast<std::chrono::seconds>(now - lastKeepAlive)
              .count() >= 2) {
        lastKeepAlive = now;
        uint8_t dummy = 0;
        listener.SendTo("stun.l.google.com", 19302, &dummy, 1);
      }
    }

    ssize_t bytesRead =
        listener.ReceiveFrom(buffer, sizeof(buffer), peerIp, peerPort);
    if (bytesRead >= static_cast<ssize_t>(sizeof(JoinRequestPacket))) {
      auto *req = reinterpret_cast<JoinRequestPacket *>(buffer);
      if (req->type == PacketType::JOIN_REQUEST) {
        if (req->protocolVersion != PROTOCOL_VERSION) {
          JoinRefusePacket refuse{PacketType::JOIN_REFUSE,
                                  RefuseReason::VERSION_MISMATCH};
          listener.SendTo(peerIp, peerPort, &refuse, sizeof(refuse));
          continue;
        }

        std::cout << "\n[+] Oyuncu baglandi: " << peerIp << ":" << peerPort
                  << "\n";

        // Kabul paketini hazırla ve güvenli olması için birkaç kez fırlat
        uint32_t seed = 424242;
        JoinAcceptPacket acceptPkt{PacketType::JOIN_ACCEPT, 1, seed};
        for (int i = 0; i < 5; i++) {
          listener.SendTo(peerIp, peerPort, &acceptPkt, sizeof(acceptPkt));
          std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        std::cout << "Oyun 1 saniye icinde basliyor...\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));

        listener.Close();

        // Oyunu Host (Player 0) olarak başlat
        Game game(0, peerIp, peerPort, localPort, seed);
        game.Start();
        break;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
}

void JoinLobby() {
  std::cout << "\n========================================\n";
  std::cout << "        MSNAKE ODAYA KATIL (JOIN)       \n";
  std::cout << "========================================\n";
  std::cout << "Oda Kodunu Girin (Orn: wKgB-Ih-Q): ";

  std::string roomCode;
  std::cin >> roomCode;

  PeerAddress hostAddr;
  if (!RoomCode::Decode(roomCode, hostAddr)) {
    std::cout << "[-] Gecersiz veya bozuk oda kodu!\n";
    return;
  }

  std::cout << "[*] Oda Cozuldu -> Hedef: " << hostAddr.ip << ":"
            << hostAddr.port << "\n";

  uint16_t myPort = 9000;
  Connection client;
  while (!client.Bind(myPort) && myPort < 9020) {
    myPort++;
  }
  client.SetNonBlocking(true);

  // Joiner da kendi STUN deliğini açar (İki taraflı UDP Hole Punching)
  std::string myPubIp;
  uint16_t myPubPort;
  StunClient::GetPublicEndpoint(client, myPubIp, myPubPort);

  std::cout << "[*] Baglanti kuruluyor (UDP Hole Punching)...\n";

  JoinRequestPacket req;
  req.type = PacketType::JOIN_REQUEST;
  req.protocolVersion = PROTOCOL_VERSION;

  char buffer[256];
  std::string senderIp;
  uint16_t senderPort = 0;
  bool accepted = false;
  uint32_t seed = 0;

  // 10 saniye boyunca (100 deneme x 100ms) delme paketleri gönder
  for (int attempt = 0; attempt < 100; attempt++) {
    // Hem çözülen hedefe, hem de aynı makinede test ediliyorsa localhost/loopback'e paket fırlat
    client.SendTo(hostAddr.ip, hostAddr.port, &req, sizeof(req));
    client.SendTo("127.0.0.1", hostAddr.port, &req, sizeof(req));

    for (int check = 0; check < 5; check++) {
      ssize_t bytesRead =
          client.ReceiveFrom(buffer, sizeof(buffer), senderIp, senderPort);
      if (bytesRead > 0) {
        auto *header = reinterpret_cast<PacketHeader *>(buffer);
        if (header->type == PacketType::JOIN_ACCEPT &&
            bytesRead >= static_cast<ssize_t>(sizeof(JoinAcceptPacket))) {
          auto *acc = reinterpret_cast<JoinAcceptPacket *>(buffer);
          seed = acc->randomSeed;
          accepted = true;
          break;
        } else if (header->type == PacketType::JOIN_REFUSE) {
          std::cout << "[-] Baglanti reddedildi!\n";
          client.Close();
          return;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (accepted)
      break;

    if (attempt % 10 == 0 && attempt > 0) {
      std::cout << "[*] Baglanti deneniyor... (" << (attempt / 10) << "s)\n";
    }
  }

  if (!accepted) {
    std::cout << "[-] Host'a ulasilamadi (Zaman asimi)!\n";
    client.Close();
    return;
  }

  std::cout << "[+] Odaya katilindi! Oyun baslatiliyor...\n";
  std::this_thread::sleep_for(std::chrono::seconds(1));

  client.Close();

  // Gerçekte hangi IP'den cevap geldiyse ona bağlan
  std::string targetIp = senderIp.empty() ? hostAddr.ip : senderIp;
  uint16_t targetPort = (senderPort == 0) ? hostAddr.port : senderPort;

  Game game(1, targetIp, targetPort, myPort, seed);
  game.Start();
}

int main() {
  while (true) {
    std::cout << "\033[2J\033[H"; // Terminali temizle
    std::cout << "======================================\n";
    std::cout << "       MULTIPLAYER SNAKE (MSNAKE)     \n";
    std::cout << "======================================\n";
    std::cout << "[1] Oda Kur (Host)\n";
    std::cout << "[2] Odaya Katil (Join with Room Code)\n";
    std::cout << "[3] Cikis\n";
    std::cout << "Seciminiz [1-3]: ";

    int choice = 0;
    if (!(std::cin >> choice)) {
      break;
    }

    if (choice == 1) {
      HostLobby();
      break;
    } else if (choice == 2) {
      JoinLobby();
      break;
    } else if (choice == 3) {
      std::cout << "Gorusmek uzere!\n";
      break;
    }
  }

  return 0;
}
