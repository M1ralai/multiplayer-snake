#include "game/snake.hpp"

Snake::Snake(int x, int y, Point direction) {
  for (int i = 0; i < BASE_LENGTH; i++) {
    this->body.push_back(Point{x, y - i});
  }
  this->direction = direction;
}

Point Snake::GetHead() const { return this->body.front(); }

const std::deque<Point> &Snake::GetBody() const { return this->body; }

void Snake::ChangeDirection(Point direction) {
  if (direction + this->direction != Direction::NONE) {
    this->direction = direction;
  }
}

void Snake::Move(bool grown) {
  this->body.push_front(this->body[0] + this->direction);
  if (!grown) {
    this->body.pop_back();
  }
}
