#include <stdio.h>

#include "pico/stdlib.h"

// int main() {
//   int delay = 1000;
//   // Initialise I/O
//   // stdio_init_all();
//   timer_hw->dbgpause = 0;

//   // initialise GPIO (Green LED connected to pin 25)
//   gpio_init(25);
//   gpio_set_dir(25, GPIO_OUT);

//   // Main Loop
//   while (1) {
//     gpio_put(25, 1);  // Set pin 25 to high
//     sleep_ms(delay/2);  // 1.0s delay
//     gpio_put(25, 0);  // Set pin 25 to low
//     sleep_ms(delay/2);  // 1.0s delay
//   }
// }

int main() {
  int delay = 1000;
  // Initialise I/O
  // stdio_init_all();
  timer_hw->dbgpause = 0;

  // initialise GPIO (Green LED connected to pin 2)
  gpio_init(2);
  gpio_set_dir(2, GPIO_OUT);

  // Main Loop
  while (1) {
    gpio_put(2, 1);  // Set pin 2 to high
    sleep_ms(delay/2);  // 1.0s delay
    gpio_put(2, 0);  // Set pin 2 to low
    sleep_ms(delay/2);  // 1.0s delay
  }
}
