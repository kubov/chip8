all:
	gcc main.c -o chip8 -ggdb -Wall -lSDL -std=c99
