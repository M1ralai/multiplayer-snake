#include "game/game.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <unistd.h>

static constexpr int TICK_RATE_MS = 250; // 0.25 saniyede bir olay işleme

Game::Game(uint8_t playerId, const std::string &peerIp, uint16_t peerPort,
           uint16_t localPort, uint32_t seed)
    : peerIp(peerIp), peerPort(peerPort), localPlayerId(playerId),
      snake0(Snake(10, CANVAS_HEIGHT / 2, Direction::RIGHT)),
      snake1(Snake(CANVAS_WIDTH - 10, CANVAS_HEIGHT / 2, Direction::LEFT)),
      randomSeed(seed) {

  nextLocalDir = (localPlayerId == 0) ? Direction::RIGHT : Direction::LEFT;
  connection.Bind(localPort);
  connection.SetNonBlocking(true);
}

Game::~Game() { Stop(); }

void Game::EnableRawMode() {
  tcgetattr(STDIN_FILENO, &origTermios);
  struct termios raw = origTermios;
  raw.c_lflag &= ~(ECHO | ICANON); // Echo ve line buffering'i kapat
  raw.c_cc[VMIN] = 0;              // Non-blocking read
  raw.c_cc[VTIME] = 1;             // 100ms timeout
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

  // İmleci gizle ve ekranı temizle
  std::cout << "\033[2J\033[?25l" << std::flush;
}

void Game::DisableRawMode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTermios);
  // İmleci tekrar göster
  std::cout << "\033[?25h" << std::flush;
}

void Game::Start() {
  isRunning = true;
  isGameOver = false;
  EnableRawMode();

  SpawnApple();

  // İlk kareyi hemen çiz
  RenderFrame();

  // Thread'leri başlatıyoruz
  inputThread = std::thread(&Game::InputLoop, this);
  networkThread = std::thread(&Game::NetworkLoop, this);
  tickThread = std::thread(&Game::TickLoop, this);

  // Tick döngüsü bitene kadar bekle
  if (tickThread.joinable())
    tickThread.join();
  if (inputThread.joinable())
    inputThread.join();
  if (networkThread.joinable())
    networkThread.join();
}

void Game::Stop() {
  if (isRunning.exchange(false)) {
    DisableRawMode();
    connection.Close();
  }
}

void Game::SpawnApple() {
  // Ortak deterministik PRNG (LCG)
  randomSeed = (randomSeed * 1103515245 + 12345) & 0x7fffffff;
  int x = 2 + (randomSeed % (CANVAS_WIDTH - 4));
  randomSeed = (randomSeed * 1103515245 + 12345) & 0x7fffffff;
  int y = 2 + (randomSeed % (CANVAS_HEIGHT - 4));
  apple = Point{x, y};
}

void Game::InputLoop() {
  char buf[3];
  while (isRunning) {
    ssize_t bytesRead = read(STDIN_FILENO, buf, sizeof(buf));
    if (bytesRead > 0) {
      char c = buf[0];
      Point newDir = Direction::NONE;

      if (c == 'w' || c == 'W')
        newDir = Direction::UP;
      else if (c == 's' || c == 'S')
        newDir = Direction::DOWN;
      else if (c == 'a' || c == 'A')
        newDir = Direction::LEFT;
      else if (c == 'd' || c == 'D')
        newDir = Direction::RIGHT;
      else if (c == '\033' && bytesRead >= 3 && buf[1] == '[') {
        // Ok tuşları
        if (buf[2] == 'A')
          newDir = Direction::UP;
        else if (buf[2] == 'B')
          newDir = Direction::DOWN;
        else if (buf[2] == 'C')
          newDir = Direction::RIGHT;
        else if (buf[2] == 'D')
          newDir = Direction::LEFT;
      } else if (c == 'q' || c == 'Q') {
        isRunning = false;
        break;
      }

      if (newDir != Direction::NONE) {
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          nextLocalDir = newDir;
        }

        // ANLIK GÖNDERİM: Tuşa basıldığı an paketi derhal karşıya fırlat!
        InputPacket pkt;
        pkt.type = PacketType::INPUT;
        pkt.playerId = localPlayerId;
        pkt.tick = currentTick;
        pkt.dirX = static_cast<int8_t>(newDir.x);
        pkt.dirY = static_cast<int8_t>(newDir.y);
        connection.SendTo(peerIp, peerPort, &pkt, sizeof(pkt));
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

void Game::NetworkLoop() {
  uint8_t buffer[256];
  std::string senderIp;
  uint16_t senderPort = 0;

  while (isRunning) {
    ssize_t bytesRead = connection.ReceiveFrom(buffer, sizeof(buffer),
                                               senderIp, senderPort);
    if (bytesRead > 0) {
      auto *header = reinterpret_cast<PacketHeader *>(buffer);
      if (header->type == PacketType::INPUT &&
          bytesRead >= static_cast<ssize_t>(sizeof(InputPacket))) {
        auto *pkt = reinterpret_cast<InputPacket *>(buffer);
        Point remoteDir{pkt->dirX, pkt->dirY};

        // Gelen yönü anlık olarak güncelle
        std::lock_guard<std::mutex> lock(stateMutex);
        if (localPlayerId == 0) {
          currentDir1 = remoteDir;
        } else {
          currentDir0 = remoteDir;
        }
      } else if (header->type == PacketType::DISCONNECT) {
        std::lock_guard<std::mutex> lock(stateMutex);
        isGameOver = true;
        gameOverMessage = "Rakip oyundan ayrıldı!";
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

void Game::TickLoop() {
  while (isRunning) {
    auto startTime = std::chrono::steady_clock::now();

    if (!isGameOver) {
      // 1. Simülasyonu 1 tick ilerlet
      {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (localPlayerId == 0) {
          if (nextLocalDir != Direction::NONE)
            currentDir0 = nextLocalDir;
        } else {
          if (nextLocalDir != Direction::NONE)
            currentDir1 = nextLocalDir;
        }

        ProcessTick(currentTick);
      }

      currentTick++;
    }

    // 2. HER OLAY / TICK SONRASI EKRANA BAS (Render Event)
    RenderFrame();

    if (isGameOver) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } else {
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - startTime)
                         .count();
      if (elapsed < TICK_RATE_MS) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(TICK_RATE_MS - elapsed));
      }
    }
  }
}

bool Game::CheckCollision(Point head, const Snake &otherSnake, bool checkSelf) {
  // Duvara çarpma
  if (head.x <= 0 || head.x >= CANVAS_WIDTH - 1 || head.y <= 0 ||
      head.y >= CANVAS_HEIGHT - 1) {
    return true;
  }
  // Karşı yılanın gövdesine çarpma
  for (const auto &segment : otherSnake.GetBody()) {
    if (head == segment)
      return true;
  }
  // Kendi gövdesine çarpma
  if (checkSelf) {
    const auto &body =
        (localPlayerId == 0) ? snake0.GetBody() : snake1.GetBody();
    for (size_t i = 1; i < body.size(); i++) {
      if (head == body[i])
        return true;
    }
  }
  return false;
}

void Game::ProcessTick(uint32_t /*tick*/) {
  snake0.ChangeDirection(currentDir0);
  snake1.ChangeDirection(currentDir1);

  Point head0 = snake0.GetHead() + snake0.GetDirection();
  Point head1 = snake1.GetHead() + snake1.GetDirection();

  bool grow0 = (head0 == apple);
  bool grow1 = (head1 == apple);

  if (grow0) {
    score0 += 10;
    SpawnApple();
  }
  if (grow1) {
    score1 += 10;
    SpawnApple();
  }

  snake0.Move(grow0);
  snake1.Move(grow1);

  // Çarpışma kontrolleri
  bool dead0 = CheckCollision(snake0.GetHead(), snake1, true);
  bool dead1 = CheckCollision(snake1.GetHead(), snake0, true);

  // Kafa kafaya çarpışma
  if (snake0.GetHead() == snake1.GetHead()) {
    dead0 = true;
    dead1 = true;
  }

  if (dead0 && dead1) {
    isGameOver = true;
    gameOverMessage = "BERABERE! (Iki yilan da carpisti)";
  } else if (dead0) {
    isGameOver = true;
    gameOverMessage = (localPlayerId == 0) ? "KAYBETTINIZ! (Oyuncu 2 Kazandi)"
                                           : "KAZANDINIZ! (Oyuncu 1 Carpisti)";
  } else if (dead1) {
    isGameOver = true;
    gameOverMessage = (localPlayerId == 1) ? "KAYBETTINIZ! (Oyuncu 1 Kazandi)"
                                           : "KAZANDINIZ! (Oyuncu 2 Carpisti)";
  }
}

void Game::RenderFrame() {
  std::lock_guard<std::mutex> lock(stateMutex);
  canvas.Clear();

  // Skor Tablosu
  std::string scoreStr = "P1 [O]: " + std::to_string(score0) +
                         " | P2 [@]: " + std::to_string(score1) +
                         " | Tick: " + std::to_string(currentTick);
  canvas.SetString(scoreStr.c_str(), 3, 0);

  // Elmayı çiz
  canvas.SetPoint(apple, '*');

  // Yılanları çiz (P1 = 'O'/'o', P2 = '@'/'#')
  canvas.SetSnake(snake0, 'O', 'o');
  canvas.SetSnake(snake1, '@', '#');

  if (isGameOver) {
    canvas.SetString("--- OYUN BITTI ---", CANVAS_WIDTH / 2 - 9,
                     CANVAS_HEIGHT / 2 - 1);
    canvas.SetString(gameOverMessage.c_str(), CANVAS_WIDTH / 2 - 16,
                     CANVAS_HEIGHT / 2 + 1);
    canvas.SetString("Cikmak icin 'Q' tusuna basin", CANVAS_WIDTH / 2 - 15,
                     CANVAS_HEIGHT / 2 + 3);
  }

  canvas.Draw();
}
