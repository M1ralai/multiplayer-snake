#include "network/upnp.hpp"
#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <sstream>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#define SSDP_IP "239.255.255.250"
#define SSDP_PORT 1900

static std::string g_controlUrl = "";
static std::string g_controlHost = "";
static uint16_t g_controlPort = 0;
static std::string g_serviceType = "urn:schemas-upnp-org:service:WANIPConnection:1";

// Basit HTTP POST isteği atan yardımcı fonksiyon
static std::string HttpPost(const std::string &host, uint16_t port,
                            const std::string &path, const std::string &soapAction,
                            const std::string &body) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return "";

  struct hostent *server = gethostbyname(host.c_str());
  if (!server) {
    close(fd);
    return "";
  }

  struct sockaddr_in servAddr {};
  servAddr.sin_family = AF_INET;
  std::memcpy(&servAddr.sin_addr.s_addr, server->h_addr, server->h_length);
  servAddr.sin_port = htons(port);

  // 1 saniye timeout koy
  struct timeval tv {
    1, 0
  };
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

  if (connect(fd, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) {
    close(fd);
    return "";
  }

  std::ostringstream ss;
  ss << "POST " << path << " HTTP/1.1\r\n";
  ss << "Host: " << host << ":" << port << "\r\n";
  ss << "User-Agent: msnake/1.0 UPnP/1.0\r\n";
  ss << "Content-Type: text/xml; charset=\"utf-8\"\r\n";
  ss << "SOAPAction: \"" << soapAction << "\"\r\n";
  ss << "Content-Length: " << body.length() << "\r\n";
  ss << "Connection: close\r\n\r\n";
  ss << body;

  std::string req = ss.str();
  send(fd, req.c_str(), req.length(), 0);

  char buffer[2048];
  std::string response = "";
  ssize_t bytes = 0;
  while ((bytes = recv(fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
    buffer[bytes] = '\0';
    response += buffer;
  }

  close(fd);
  return response;
}

// SSDP ile modemin UPnP Control URL'ini bulur
static bool DiscoverRouter(std::string &outHost, uint16_t &outPort,
                           std::string &outPath) {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0)
    return false;

  // 2 saniye timeout
  struct timeval tv {
    2, 0
  };
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  int broadcast = 1;
  setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

  std::string ssdpMsg =
      "M-SEARCH * HTTP/1.1\r\n"
      "HOST: 239.255.255.250:1900\r\n"
      "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n"
      "MAN: \"ssdp:discover\"\r\n"
      "MX: 2\r\n\r\n";

  struct sockaddr_in destAddr {};
  destAddr.sin_family = AF_INET;
  destAddr.sin_port = htons(SSDP_PORT);
  inet_pton(AF_INET, SSDP_IP, &destAddr.sin_addr);

  sendto(fd, ssdpMsg.c_str(), ssdpMsg.length(), 0,
         (struct sockaddr *)&destAddr, sizeof(destAddr));

  // Doğrudan varsayılan gateway'e (192.168.1.1) de tekil SSDP fırlat
  struct sockaddr_in gatewayAddr {};
  gatewayAddr.sin_family = AF_INET;
  gatewayAddr.sin_port = htons(SSDP_PORT);
  inet_pton(AF_INET, "192.168.1.1", &gatewayAddr.sin_addr);
  sendto(fd, ssdpMsg.c_str(), ssdpMsg.length(), 0,
         (struct sockaddr *)&gatewayAddr, sizeof(gatewayAddr));

  char buffer[4096];
  struct sockaddr_in senderAddr {};
  socklen_t addrLen = sizeof(senderAddr);

  ssize_t bytes = recvfrom(fd, buffer, sizeof(buffer) - 1, 0,
                           (struct sockaddr *)&senderAddr, &addrLen);
  close(fd);

  if (bytes <= 0) {
    return false;
  }
  buffer[bytes] = '\0';
  std::string resp(buffer);

  // LOCATION header'ını bul (örn: http://192.168.1.1:1780/rootDesc.xml)
  size_t locPos = resp.find("LOCATION:");
  if (locPos == std::string::npos) {
    locPos = resp.find("location:");
  }
  if (locPos == std::string::npos)
    return false;

  size_t start = resp.find("http://", locPos);
  if (start == std::string::npos)
    return false;
  start += 7; // "http://" atla

  size_t end = resp.find("\r\n", start);
  std::string fullUrl = resp.substr(start, end - start);

  size_t colon = fullUrl.find(':');
  size_t slash = fullUrl.find('/');

  if (colon != std::string::npos && slash != std::string::npos) {
    outHost = fullUrl.substr(0, colon);
    outPort = std::stoi(fullUrl.substr(colon + 1, slash - colon - 1));
    outPath = fullUrl.substr(slash);
  } else if (slash != std::string::npos) {
    outHost = fullUrl.substr(0, slash);
    outPort = 80;
    outPath = fullUrl.substr(slash);
  } else {
    return false;
  }

  return true;
}

bool UPnP::ForwardPort(uint16_t port, const std::string &localIp,
                       std::string &outPublicIp) {
  std::string host, path;
  uint16_t routerPort = 0;

  if (!DiscoverRouter(host, routerPort, path)) {
    return false;
  }

  g_controlHost = host;
  g_controlPort = routerPort;
  g_controlUrl = "/ctl/IPConn"; // Standart UPnP control path

  // 1. Modeme Port Açma SOAP Emri Gönder
  std::string soapAction = g_serviceType + "#AddPortMapping";
  std::ostringstream body;
  body << "<?xml version=\"1.0\"?>\r\n"
       << "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
          "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
       << "<s:Body>\r\n"
       << "<u:AddPortMapping xmlns:u=\"" << g_serviceType << "\">\r\n"
       << "  <NewRemoteHost></NewRemoteHost>\r\n"
       << "  <NewExternalPort>" << port << "</NewExternalPort>\r\n"
       << "  <NewProtocol>UDP</NewProtocol>\r\n"
       << "  <NewInternalPort>" << port << "</NewInternalPort>\r\n"
       << "  <NewInternalClient>" << localIp << "</NewInternalClient>\r\n"
       << "  <NewEnabled>1</NewEnabled>\r\n"
       << "  <NewPortMappingDescription>msnake</NewPortMappingDescription>\r\n"
       << "  <NewLeaseDuration>3600</NewLeaseDuration>\r\n"
       << "</u:AddPortMapping>\r\n"
       << "</s:Body>\r\n"
       << "</s:Envelope>\r\n";

  std::string res =
      HttpPost(g_controlHost, g_controlPort, g_controlUrl, soapAction, body.str());

  // 2. Modemin Dış (WAN) IP'sini Al
  std::string getIpAction = g_serviceType + "#GetExternalIPAddress";
  std::string getIpBody =
      "<?xml version=\"1.0\"?>\r\n"
      "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
      "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
      "<s:Body>\r\n"
      "<u:GetExternalIPAddress xmlns:u=\"" +
      g_serviceType +
      "\">\r\n"
      "</u:GetExternalIPAddress>\r\n"
      "</s:Body>\r\n"
      "</s:Envelope>\r\n";

  std::string ipRes = HttpPost(g_controlHost, g_controlPort, g_controlUrl,
                               getIpAction, getIpBody);

  size_t ipStart = ipRes.find("<NewExternalIPAddress>");
  size_t ipEnd = ipRes.find("</NewExternalIPAddress>");
  if (ipStart != std::string::npos && ipEnd != std::string::npos) {
    ipStart += 22; // "<NewExternalIPAddress>" uzunluğu
    outPublicIp = ipRes.substr(ipStart, ipEnd - ipStart);
    return true;
  }

  return false;
}

void UPnP::RemovePort(uint16_t port) {
  if (g_controlHost.empty())
    return;

  std::string soapAction = g_serviceType + "#DeletePortMapping";
  std::ostringstream body;
  body << "<?xml version=\"1.0\"?>\r\n"
       << "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
          "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
       << "<s:Body>\r\n"
       << "<u:DeletePortMapping xmlns:u=\"" << g_serviceType << "\">\r\n"
       << "  <NewRemoteHost></NewRemoteHost>\r\n"
       << "  <NewExternalPort>" << port << "</NewExternalPort>\r\n"
       << "  <NewProtocol>UDP</NewProtocol>\r\n"
       << "</u:DeletePortMapping>\r\n"
       << "</s:Body>\r\n"
       << "</s:Envelope>\r\n";

  HttpPost(g_controlHost, g_controlPort, g_controlUrl, soapAction, body.str());
}
