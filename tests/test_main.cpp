#include "tests.hpp"
#include <iostream>

int main() {
  std::cout << "======================================\n";
  std::cout << "   RUNNING ALL NETWORK SUITE TESTS    \n";
  std::cout << "======================================\n";

  TestRoomCode();
  TestConnection();
  TestStun();
  TestUPnP();

  std::cout << "\n======================================\n";
  std::cout << "   ALL TESTS PASSED SUCCESSFULLY! 🎉  \n";
  std::cout << "======================================\n";
  return 0;
}
