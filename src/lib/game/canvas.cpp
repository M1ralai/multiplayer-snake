#include "game/canvas.hpp"
#include <iostream>

Canvas::Canvas() { this->Clear(); }

void Canvas::SetBorder() {
  for (int i = 0; i < CANVAS_HEIGHT; i++) {
    this->pixels[i][0] = '|';
    this->pixels[i][CANVAS_WIDTH - 1] = '|';
  }
  for (int i = 0; i < CANVAS_WIDTH; i++) {
    this->pixels[0][i] = '-';
    this->pixels[CANVAS_HEIGHT - 1][i] = '-';
  }
}

void Canvas::Clear() {
  for (int i = 0; i < CANVAS_HEIGHT; i++) {
    for (int j = 0; j < CANVAS_WIDTH; j++) {
      this->pixels[i][j] = ' ';
    }
  }
  this->SetBorder();
}

#include <unistd.h>

void Canvas::Draw() {
  std::string output;
  output.reserve(CANVAS_HEIGHT * (CANVAS_WIDTH + 1) + 16);
  output += "\033[H"; // Cursor'ı sol üste taşı (ekranı silmeden üstüne yaz)

  for (int i = 0; i < CANVAS_HEIGHT; i++) {
    output.append(this->pixels[i], CANVAS_WIDTH);
    output += '\n';
  }

  // Tüm kareyi tek bir atomik write syscall'u ile bas (0 Titreme)
  write(STDOUT_FILENO, output.data(), output.size());
}

void Canvas::SetSnake(const Snake &snake, char head, char body) {
  const std::deque<Point> &b = snake.GetBody();
  if (b.empty()) {
    return;
  }
  this->SetPoint(b[0], head);
  for (size_t i = 1; i < b.size(); i++) {
    SetPoint(b[i], body);
  }
}

void Canvas::SetPixel(int x, int y, char ch) {
  if (x < 0 || x >= CANVAS_WIDTH || y < 0 || y >= CANVAS_HEIGHT) {
    return;
  }
  this->pixels[y][x] = ch; // [Y][X]
}

void Canvas::SetPoint(const Point point, char ch) {
  this->SetPixel(point.x, point.y, ch);
}

void Canvas::SetString(const char *str, int x, int y) {
  if (strlen(str) + x > CANVAS_WIDTH) {
    return;
  }
  for (int i = 0; str[i] != '\0'; i++, x++) {
    this->pixels[y][x] = str[i];
  }
}
