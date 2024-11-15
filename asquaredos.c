#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

#include "kern/context_switch.h"

#define RAM_ADDR (SRAM_BASE + (128 * 1024))
#define FLASH_ADDR (XIP_BASE + (128 * 1024))

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
  printf("after foo\n");

}
