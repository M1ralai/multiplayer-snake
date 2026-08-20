#ifndef HCB_MSNAKE_SNAKE
#define HCB_MSNAKE_SNAKE

#include "utils/utils.hpp"
#include <deque>

#define BASE_LENGTH 3

namespace Direction {
inline constexpr Point UP{0, -1};
inline constexpr Point DOWN{0, 1};
inline constexpr Point LEFT{-1, 0};
inline constexpr Point RIGHT{1, 0};
inline constexpr Point NONE{0, 0};
} // namespace Direction

class Snake {
private:
  Point direction;
  std::deque<Point> body;
  bool isAlive{true};

public:
  Snake(int x, int y, Point direction);
  Point GetHead() const;
  const std::deque<Point> &GetBody() const;
  Point GetDirection() const { return direction; }
  bool IsAlive() const { return isAlive; }
  void SetAlive(bool alive) { isAlive = alive; }
  void ChangeDirection(Point direction);
  void Move(bool grown);
};

#endif // !HCB_MSNAKE
