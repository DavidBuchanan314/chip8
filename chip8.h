#include <stdint.h>

#define CHIP8_WIDTH 64
#define CHIP8_HEIGHT 32
#define CHIP8_TICKS_PER_FRAME 50

uint8_t chip8_keystate[0xF];

uint8_t chip8_lastkey;

int chip8_waiting;

void chip8_init();

void chip8_loadfile(char *filename);

uint8_t * chip8_getframe();

int chip8_getstatus();

void chip8_playpause();

void chip8_tick();
