#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "chip8.h"

#define MEM(addr) (memory[(addr)&0xFFF])

#define KK MEM(PC+1)
#define X (MEM(PC) & 0xF)
#define Y (KK >> 4)
#define N (KK & 0xF)
#define NNN ((uint16_t) X<<8 | KK)

#define Vx V[X]
#define Vy V[Y]
#define V0 V[0]
#define Vf V[0xF]

static uint8_t SP, DT, ST, V[0x10], memory[0x1000];
static uint8_t screen[CHIP8_WIDTH*CHIP8_HEIGHT];
static uint16_t I, PC, stack[0x100];
static int running = 0;

uint8_t fontdata[80] = { 
	0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
	0x20, 0x60, 0x20, 0x20, 0x70, // 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
	0x90, 0x90, 0xF0, 0x10, 0x10, // 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
	0xF0, 0x10, 0x20, 0x40, 0x40, // 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
	0xF0, 0x90, 0xF0, 0x90, 0x90, // A
	0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
	0xF0, 0x80, 0x80, 0x80, 0xF0, // C
	0xE0, 0x90, 0x90, 0x90, 0xE0, // D
	0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
	0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

void chip8_init()
{
	memset(screen, 0, sizeof(screen));
	memset(memory, 0, sizeof(memory));
	memset(stack, 0, sizeof(stack));
	memset(V, 0, sizeof(V));
	I = 0;
	PC = 0x200;
	SP = 0;
	DT = 0;
	ST = 0;
	chip8_waiting = 0;
	
	memcpy(memory, fontdata, sizeof fontdata);
}

void chip8_loadfile(char *filename)
{
	chip8_init();

	printf("Loading file: %s\n", filename);
	FILE *romfile = fopen(filename, "rb");
	
	if (romfile == NULL) {
		printf("Unable to open file.\n");
		return;
	}
	
	size_t size = fread(memory + 0x200, sizeof(uint8_t), 0xE00, romfile);
	
	if (ferror(romfile)) {
		printf("Error reading file\n");
		return;
	}
	
	printf("%u bytes loaded.\n", (unsigned int) size);
	if (!feof(romfile)) printf("Warning: input file truncated to 0xE00 bytes.\n");
	fclose(romfile);
}

uint8_t * chip8_getframe()
{
	return screen;
}

int chip8_getstatus()
{
	return running;
}

void chip8_playpause()
{
	running = !running;
}

int chip8_rendersprite(int x, int y, uint16_t ptr, int n) // TODO maybe switch to bitpacked graphics for faster rendering
{
	int collision = 0;
	for (int i=0; i<n; i++) {
		for (int j=0; j<8; j++) {
			int val = MEM(ptr+i)>>(7-j) & 1;
			int scrptr = CHIP8_WIDTH*((y+i)%CHIP8_HEIGHT)+((x+j)%CHIP8_WIDTH);
			collision |= screen[scrptr] && val;
			screen[scrptr] ^= val;
		}
	}
	return collision;
}

void chip8_tick() {
	if (!running) return;
	
	if (DT) DT--;
	if (ST) ST--;
	
	for (int tick = 0; tick < CHIP8_TICKS_PER_FRAME; tick++) {
		//printf("PC: 0x%03x, Op: 0x%02x%02x, I: 0x%04x Vx: 0x%02x, Vy: 0x%02x\n", PC, memory[PC], memory[PC+1], I, Vx, Vy);
		switch(MEM(PC) >> 4) {
		case 0x0:
			switch (KK) {
			case 0xE0: // 00E0 - CLS
				memset(screen, 0, CHIP8_WIDTH*CHIP8_HEIGHT);
				break; 
			case 0xEE: // 00EE - RET
				PC = stack[--SP];
				break;
			default:
				printf("Not implemented\n");
			}
			break;
		case 0x1: // 1nnn - JP addr
			PC = NNN - 2;
			break;
		case 0x2: // 2nnn - CALL addr
			stack[SP++] = PC;
			PC = NNN - 2;
			break;
		case 0x3: // 3xkk - SE Vx, byte
			if (Vx == KK) PC += 2;
			break;
		case 0x4: // 4xkk - SNE Vx, byte
			if (Vx != KK) PC += 2;
			break;
		case 0x5: // 5xy0 - SE Vx, Vy
			if (Vx == Vy) PC += 2;
			break;
		case 0x6: // 6xkk - LD Vx, byte
			Vx = KK;
			break;
		case 0x7: // 7xkk - ADD Vx, byte
			Vx += KK;
			break;
		case 0x8:
			switch (N) {
			case 0x0: // 8xy0 - LD Vx, Vy
				Vx = Vy;
				break;
			case 0x1: // 8xy1 - OR Vx, Vy
				Vx |= Vy;
				break;
			case 0x2: // 8xy2 - AND Vx, Vy
				Vx &= Vy;
				break;
			case 0x3: // 8xy3 - XOR Vx, Vy
				Vx ^= Vy;
				break;
			case 0x4: // 8xy4 - ADD Vx, Vy
				Vf = Vx + Vy > 0xFF;
				Vx += Vy;
				break;
			case 0x5: // 8xy5 - SUB Vx, Vy
				Vf = Vx > Vy;
				Vx -= Vy;
				break;
			case 0x6: // 8xy6 - SHR Vx {, Vy}
				Vf = Vx & 1;
				Vx /= 2;
				break;
			case 0x7: // 8xy7 - SUBN Vx, Vy
				Vf = Vx < Vy;
				Vx = Vy - Vx;
				break;
			case 0xE: // 8xyE - SHL Vx {, Vy}
				Vf = Vx >> 7;
				Vx *= 2;
				break;
			default:
				printf("Not implemented\n");
			}
			break;
		case 0x9: // 9xy0 - SNE Vx, Vy
			if (Vx != Vy) PC += 2;
			break;
		case 0xA: // Annn - LD I, addr
			I = NNN;
			break;
		case 0xB: // Bnnn - JP V0, addr
			PC = NNN + V0 - 2;
			break;
		case 0xC: // Cxkk - RND Vx, byte
			Vx = rand() & KK;
			break;
		case 0xD: // Dxyn - DRW Vx, Vy, nibble
			Vf = chip8_rendersprite(Vx, Vy, I, N);
			break;
		case 0xE:
			switch (KK) {
			case 0x9E: // Ex9E - SKP Vx
				if (chip8_keystate[Vx]) PC += 2;
				break;
			case 0xA1: // ExA1 - SKNP Vx
				if (!chip8_keystate[Vx]) PC += 2;
				break;
			default:
				printf("Not implemented\n");
			}
			break;
		case 0xF:
			switch (KK) {
			case 0x07: // Fx07 - LD Vx, DT
				Vx = DT;
				break;
			case 0x0A: // Fx0A - LD Vx, K
				if (chip8_waiting) {
					chip8_waiting = 0;
					Vx = chip8_lastkey;
				} else {
					chip8_waiting = 1;
					running = 0;
					return;
				}
				break;
			case 0x15: // Fx15 - LD DT, Vx
				DT = Vx;
				break;
			case 0x18: // Fx18 - LD ST, Vx
				ST = Vx;
				break;
			case 0x1E: // Fx1E - ADD I, Vx
				I += Vx;
				break;
			case 0x29: // Fx29 - LD F, Vx
				I = Vx * 5;
				break;
			case 0x33: // Fx33 - LD B, Vx
				MEM(I+0) = Vx / 100;
				MEM(I+1) = (Vx/10) % 10;
				MEM(I+2) = Vx % 10;
				break;
			case 0x55: // Fx55 - LD [I], Vx
				for (uint16_t i=0; i <= X; i++) MEM(I+i) = V[i];
				break;
			case 0x65:; // Fx65 - LD Vx, [I]
				for (uint16_t i=0; i <= X; i++) V[i] = MEM(I+i);
				break;
			default:
				printf("Not implemented\n");
			}
			break;
		default:
			printf("Not implemented\n");
		}
		PC += 2;
		PC &= 0xFFF;
		I &= 0xFFF;
	}
}
