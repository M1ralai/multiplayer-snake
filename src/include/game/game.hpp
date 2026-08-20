#ifndef HCB_MSNAKE_GAME
#define HCB_MSNAKE_GAME

#include "game/canvas.hpp"
#include "game/snake.hpp"
#include "network/connection.hpp"
#include "network/packet.hpp"
#include "utils/thread_safe_queue.hpp"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <termios.h>
#include <thread>
#include <unordered_map>

class Game {
private:
  // Ağ Ayarları
  Connection connection;
  std::string peerIp;
  uint16_t peerPort{0};
  uint8_t localPlayerId{0}; // 0 = Host, 1 = Joiner

  // Oyun Durumu
  Canvas canvas;
  Snake snake0; // Oyuncu 0 (Host)
  Snake snake1; // Oyuncu 1 (Joiner)
  Point apple{CANVAS_WIDTH / 2, CANVAS_HEIGHT / 2};
  int score0{0};
  int score1{0};
  uint32_t currentTick{0};
  uint32_t randomSeed{12345};

  // Senkronizasyon ve Thread Kontrolü
  std::atomic<bool> isRunning{false};
  std::atomic<bool> isGameOver{false};
  std::string gameOverMessage;
  std::mutex stateMutex;

  // Girdiler
  Point nextLocalDir{Direction::NONE};
  Point currentDir0{Direction::RIGHT};
  Point currentDir1{Direction::LEFT};

  // Lockstep / Tick Girdi Tamponu
  std::mutex inputBufferMutex;
  std::unordered_map<uint32_t, Point> remoteInputs; // Tick -> Remote Direction

  // Thread Nesneleri
  std::thread inputThread;
  std::thread networkThread;
  std::thread tickThread;

  // Terminal Yönetimi
  struct termios origTermios {};
  void EnableRawMode();
  void DisableRawMode();

  // İş Parçacığı Döngüleri
  void InputLoop();
  void NetworkLoop();
  void TickLoop();
  void RenderFrame();

  // Oyun Mantığı
  void SpawnApple();
  void ProcessTick(uint32_t tick);
  bool CheckCollision(Point head, const Snake &otherSnake, bool checkSelf);

public:
  Game(uint8_t playerId, const std::string &peerIp, uint16_t peerPort,
       uint16_t localPort, uint32_t seed);
  ~Game();

  void Start();
  void Stop();
};

#endif // !HCB_MSNAKE_GAME
