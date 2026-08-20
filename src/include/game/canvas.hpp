#ifndef HCB_MSNAKE_CANVAS

#define HCB_MSNAKE_CANVAS

#define CANVAS_WIDTH 64
#define CANVAS_HEIGHT 32

#include "snake.hpp"
#include "utils/utils.hpp"

class Canvas {
private:
  char pixels[CANVAS_HEIGHT][CANVAS_WIDTH];
  void SetBorder();

public:
  Canvas();
  void Clear();
  void Draw();

  void SetSnake(const Snake &snake, char head, char body);

  void SetPixel(int x, int y, char ch);
  void SetPoint(Point point, char ch);
  void SetString(const char *str, int x, int y);
};

#endif // !HCB_MSNAKE
