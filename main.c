#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <SDL/SDL.h>
#include <time.h>

SDL_Surface* window;

void putpixel(SDL_Surface *surface, uint16_t x, uint16_t y, uint32_t value)
{
    uint8_t *pixel = (uint8_t *)surface->pixels;
    pixel += (y * surface->pitch) + (x * sizeof(uint32_t));
    (*((uint32_t *)pixel)) = value;
}

typedef struct
{
    uint8_t vs[16];
    uint16_t i;

    uint8_t mem[0xfff];
    uint16_t stack[0xff];
    uint16_t pc;
    uint16_t sp;
} cpu;

uint8_t safe_add(uint8_t a, uint8_t b)
{
    uint8_t tmp = a+b;
    return tmp < a;
}

uint8_t safe_sub(uint8_t a, uint8_t b)
{
    return a < b;
}

void clear_screen()
{
}

void step(cpu* chip8)
{
    uint16_t opcode = ((chip8->mem[chip8->pc])<< 8) + chip8->mem[chip8->pc+1];
    uint8_t fam = opcode >> 12;
    uint16_t addr;
    uint8_t do_branch = false;
    printf("opcode -> %.0x\n", opcode);

    switch(fam)
    {
    case 0x00:
        if (opcode == 0x00e0)
        {
            clear_screen();
        }
        if (opcode == 0x00ee){
            addr = chip8->stack[chip8->sp-1];
            chip8->sp--;
            do_branch = true;
        }
        break;
    case 0x01:
        chip8->pc = opcode & 0x0fff;
        break;
    case 0x02:
        chip8->stack[chip8->sp] = chip8->pc + 2;
        chip8->sp++;
        addr = opcode & 0xfff;
        do_branch = true;
        break;
    case 0x03:
        if (chip8->vs[(opcode & 0x0f00)>>8] == (uint8_t)(0x00ff & opcode))
        {
            addr = chip8->pc + 4;
            do_branch = true;
        }
        break;
    case 0x04:
        if (chip8->vs[(opcode & 0x0f00)>>8] != (uint8_t)(0x00ff & opcode))
        {
            addr = chip8->pc+=4;
            do_branch = true;
        }
        break;
    case 0x05:
        if (chip8->vs[(opcode & 0x0f00)>>8] == chip8->vs[(opcode & 0x00f0)>>4])
        {
            addr = chip8->pc+=4;
            do_branch = true;
        }
        break;
    case 0x06:
        chip8->vs[(opcode & 0x0f00) >> 8] = opcode & 0x00ff;
        break;
    case 0x07:
        chip8->vs[(opcode & 0x0f00) >> 8] += opcode & 0x00ff;
        break;
    case 0x08:
        switch (opcode & 0x000f)
        {
        case 0x00: chip8->vs[(opcode & 0x0f00)>>8]  = chip8->vs[(opcode & 0x00f0)>>4]; break;
        case 0x01: chip8->vs[(opcode & 0x0f00)>>8] |= chip8->vs[(opcode & 0x00f0)>>4]; break;
        case 0x02: chip8->vs[(opcode & 0x0f00)>>8] &= chip8->vs[(opcode & 0x00f0)>>4]; break;
        case 0x03: chip8->vs[(opcode & 0x0f00)>>8] ^= chip8->vs[(opcode & 0x00f0)>>4]; break;
        case 0x04: chip8->vs[(opcode & 0x0f00)>>8] += chip8->vs[(opcode & 0x00f0)>>4];
            if (safe_add(chip8->vs[(opcode & 0x0f00)>>8], chip8->vs[(opcode & 0x00f0)>>4]))
                chip8->vs[0xf] = 1;
            else
                chip8->vs[0xf] = 0;
            break;
        case 0x05:
            chip8->vs[(opcode & 0x0f00)>>8] -= chip8->vs[(opcode & 0x00f0)>>4];
            if (safe_sub(chip8->vs[(opcode & 0x0f00)>>8], chip8->vs[(opcode & 0x00f0)>>4]))
                chip8->vs[0xf] = 0;
            else
                chip8->vs[0xf] = 1;
            break;
        case 0x06:
            chip8->vs[0xf] = chip8->vs[(opcode & 0x00f0)>>4] & 0x1;
            chip8->vs[(opcode & 0x0f00)>>8] = chip8->vs[(opcode & 0x00f0)>>4] >> 1;
            break;
        case 0x07:
            chip8->vs[(opcode & 0x0f00)>>8] = chip8->vs[(opcode & 0x00f0)>>4] - chip8->vs[(opcode & 0x0f00)>>8];
            if (safe_sub(chip8->vs[(opcode & 0x00f0)>>4],chip8->vs[(opcode & 0x0f00)>>8]))
                chip8->vs[0xf] = 0;
            else
                chip8->vs[0xf] = 1;
            break;
        case 0x0e:
            chip8->vs[0xf] = (chip8->vs[(opcode & 0x00f0)>>4] & 0x80) >> 7;
            chip8->vs[(opcode & 0x0f00)>>8] = chip8->vs[(opcode & 0x00f0)>>4] << 1;
            break;
        }
        break;
    case 0x09:
        if (chip8->vs[(opcode & 0x0f00)>>8] != chip8->vs[(opcode & 0x00f0)>>4])
        {
            addr = chip8->pc+=4;
            do_branch = true;
        }
        break;
    case 0x0a:
        chip8->i = opcode & 0x0fff;
        break;
    case 0x0b:
        addr = (opcode & 0x0fff) + chip8->vs[0];
        do_branch = true;
        break;
    case 0x0c:
        chip8->vs[opcode & (0x0f00 >> 8)] = (uint8_t)rand();
        break;
    case 0x0d: printf("NOT IMPLEMENTED\n"); break;
    case 0x0e: printf("NOT IMPLEMENTED\n"); break;
    case 0x0f: printf("NOT IMPLEMENTED\n"); break;
    }

    if (do_branch)
    {
        chip8->pc = addr;
    }
    else
    {
        chip8->pc += 2;
    }

}

void dump(cpu* chip8)
{
    for (int i=0;i<16;i++)
    {
        printf("v[%i] = %i\n", i, chip8->vs[i]);
    }
}

int main(int argc, char** argv)
{
    cpu chip8;

    chip8.pc = 0x0200;
    chip8.sp = 0;

    srand(time(NULL));
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_SetVideoMode(640,320,32,SDL_SWSURFACE);
    FILE *img = fopen(argv[1], "rb");
    size_t size;

    fseek(img, 0, SEEK_END);
    size = ftell(img);
    rewind(img);

    fread(chip8.mem + 0x0200, 1, size, img);
    fclose(img);
    char c;

    while((c = getchar()) != 'x')
    {
        step(&chip8);
        dump(&chip8);
    }

    return 0;
}
