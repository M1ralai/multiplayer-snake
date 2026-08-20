#include "network/upnp.hpp"
#include <iostream>

void TestUPnP() {
  std::cout << "\n--- Testing UPnP Router Port Forwarding ---\n";
  std::cout << "[TEST] Discovering Local Router via SSDP (239.255.255.250:1900)... ";

  std::string wanIp;
  bool success = UPnP::ForwardPort(8080, "127.0.0.1", wanIp);

  if (success) {
    std::cout << "SUCCESS! ✅\n";
    std::cout << "       -> Modemin WAN Dış IP'si: " << wanIp << "\n";
    std::cout << "       -> UDP 8080 Portu Modemde Basariyla Acildi! 🎉\n";
  } else {
    std::cout << "DISABLED / NOT FOUND ⚠️\n";
    std::cout << "       -> Modemde UPnP kapali olabilir (Modem arayuzunden acilabilir).\n";
  }
}
