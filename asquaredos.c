#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

#define RAM_ADDR (SRAM_BASE + (64 * 1024))
#define FLASH_ADDR (XIP_BASE + (64 * 1024))

int main () {
  /* Load the program into flash memory with the following command: */
  // picotool load -n -o 0x10040000 <filename>
  stdio_init_all();

  gpio_init(25);
  gpio_set_dir(25, GPIO_OUT);

  timer_hw->dbgpause = 0;
  printf("Hello there, now loading the program!\n");
  sleep_ms(3000);
  printf("After sleep!\n");
  
  memcpy((void *)RAM_ADDR, (void *)FLASH_ADDR, 24200);
  printf("After memcpy!\n");

  int *ram_base = RAM_ADDR;
  for (int i = 0; i < 20; i++) {
    printf("%x\n", *(ram_base + i));
  }

  // void (*foo)(void) = (void (*)())(RAM_ADDR + 0x100);
  typedef void (*EntryFn) (void);
  EntryFn foo = (EntryFn)((FLASH_ADDR) | 1);
  printf("foo is: %p\n", foo);
  foo();
  // goto *foo;

  printf("after foo\n");
  
  // asm("MOV R4, %0");

  // asm("MOV R4, #0x2");
  // asm("LSL R4, #0x14");

  // asm("MOV R3, #0x1");
  // asm("LSL R3, #0x1c");

  // asm("ADD R4, R3");
  // asm("BX R4");
}



// #include <stdio.h>

// #include "pico/stdlib.h"

// int main() {
//   int delay = 1000;
//   // Initialise I/O
//   stdio_init_all();
//   timer_hw->dbgpause = 0;

//   // initialise GPIO (Green LED connected to pin 25)
//   gpio_init(25);
//   gpio_set_dir(25, GPIO_OUT);

//   // Main Loop
//   while (1) {
//     gpio_put(25, 1);  // Set pin 25 to high
//     // printf("LED ON!\n");
//     sleep_ms(delay);  // 0.5s delay
//     // busy_wait_ms(delay);

//     gpio_put(25, 0);  // Set pin 25 to low
//     // printf("LED OFF!\n");
//     sleep_ms(delay);  // 0.5s delay
//     // busy_wait_ms(delay);
//     // delay += 100;
//   }
// }

// size before changes = 000fb8