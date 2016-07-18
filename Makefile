all:
	gcc `pkg-config --cflags gtk+-3.0` main.c chip8.c -o main `pkg-config --libs gtk+-3.0` -Ofast -Wall
