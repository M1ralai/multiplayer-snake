#ifndef HCB_MSNAKE_UPNP
#define HCB_MSNAKE_UPNP

#include <cstdint>
#include <string>

namespace UPnP {
// Modeme otomatik olarak UPnP üzerinden UDP port yönlendirmesi yaptırır
// ve modemin gerçek Dış (WAN) IP adresini döner
bool ForwardPort(uint16_t port, const std::string &localIp,
                 std::string &outPublicIp);

// Oyun bittiğinde port yönlendirmesini temizler
void RemovePort(uint16_t port);
} // namespace UPnP

#endif // !HCB_MSNAKE_UPNP
