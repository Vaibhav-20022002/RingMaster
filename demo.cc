#include <iostream>
#include <thread>

#include "RingMaster.hh"

int main() {
  RingMaster<int, 8> buf;

  std::thread producer([&] {
    for (int i = 0; i < 100; ++i) {
      while (!buf.push(i)) {
        ; // buffer full, busy-wait
      }
    }
  });

  std::thread consumer([&] {
    int value;
    int count = 0;
    while (count < 100) {
      if (buf.pop(value)) {
        std::cout << "Got: " << value << "\n";
        ++count;
      }
      // else buffer empty, busy-wait
    }
  });

  producer.join();
  consumer.join();

  std::cout << "The number should be incremental and ring buffer is working fine as intended!\n";
  return 0;
}
