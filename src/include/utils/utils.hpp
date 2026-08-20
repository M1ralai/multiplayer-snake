#ifndef HCB_MSNAKE_POINT
#define HCB_MSNAKE_POINT

struct Point {
  int x;
  int y;

  Point operator+(const Point &other) const {
    return {x + other.x, y + other.y};
  }
  bool operator==(const Point &other) const {
    return x == other.x && y == other.y;
  }
};

#endif // !HCB_MSNAKE_POINT
