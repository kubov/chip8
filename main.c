#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <SDL/SDL.h>
#include <time.h>
#include <unistd.h>

SDL_Surface* window;

void putpixel(SDL_Surface *surface, uint16_t x, uint16_t y, uint32_t value)
{
    uint8_t *pixel = (uint8_t *)surface->pixels;
    pixel += (y * surface->pitch) + (x * sizeof(uint32_t));
    (*((uint32_t *)pixel)) = value;
}

void putpixel8(SDL_Surface *surface, uint16_t x, uint16_t y, uint32_t value)
{

    for (int i = x * 10; i < x*10 + 10; i++)
    {
        for (int j = y * 10; j < y*10 + 10; j++)
            putpixel(surface,i,j,value);
    }
}

void putpixel_c8(SDL_Surface *surface, uint8_t x, uint8_t y, uint8_t value)
{
    for (uint32_t xx = x*10; xx < (x+1)*10; xx++)
    {
        for (uint32_t yy = y * 10; yy < (y+1)*10; yy++)
        {
            if (value > 0)
                putpixel(surface, xx, yy, 0xFFFFFF);
            else
                putpixel(surface, xx, yy, 0);
        }
    }
}


uint32_t getpixel(SDL_Surface *surface, uint16_t x, uint16_t y)
{
    uint8_t *pixel = (uint8_t *)surface->pixels;
    pixel += (y * surface->pitch) + (x * sizeof(uint32_t));
    return (*((uint32_t *)pixel));
}

typedef struct
{
    uint8_t vs[16];
    uint16_t i;

    uint8_t mem[0xfff];
    uint16_t stack[0xff];
    uint16_t pc;
    uint16_t sp;
    uint8_t deley_timer;
    uint8_t sound_timer;
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
    SDL_FillRect(window, NULL, 0x000);
}

uint8_t parse_int(uint16_t a)
{
    if (a >= '0' && a <= '9')
    {
        return a - 0x30;
    }
    else
    {
        return 10 + a - 0x61;
    }
}

void draw_sprite(cpu *chip8, uint8_t x, uint8_t y, uint8_t n)
{
    chip8->vs[0xf] = 0;
    for (uint8_t i=0; i<n; i++)
    {
        uint8_t tmp = chip8->mem[chip8->i + i];

        for (uint8_t m=0;m<8;m++)
        {
            uint8_t val = (tmp & (0x1 << m)) >> m;
            uint32_t last = getpixel(window, x, y);

            if (last != val*0xffffff)
            {
                chip8->vs[0xf] = 1;
            }
            if (val)
            {
                putpixel_c8(window, x + (8-m), y + i, 0xffffff ^ last);
            }
            else
            {
                putpixel_c8(window, x + (8-m), y + i, 0x00 ^ last);
            }
        }
    }
}

void step(cpu* chip8)
{
    uint16_t opcode = ((chip8->mem[chip8->pc])<< 8) + chip8->mem[chip8->pc+1];
    uint8_t fam = opcode >> 12;
    uint16_t addr;
    uint8_t do_branch = false, tmp, tmp1;
    SDL_Event event;
    printf("opcode -> %.6x\n", opcode);

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
        addr = opcode & 0x0fff;
        do_branch = true;
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
    case 0x0d: draw_sprite(chip8, chip8->vs[(opcode & 0x0f00) >> 8], chip8->vs[(opcode & 0x00f0) >> 4], opcode & 0xf); break;
    case 0x0e:
        tmp = chip8->vs[(opcode & 0x0f00)>>8];

        uint32_t lol = 500;
        while (lol--)
        {
            SDL_PollEvent( &event );
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
            {
                tmp1 = parse_int(event.key.keysym.sym);
                break;
            }
            usleep(500);
        }

        if ((tmp1 == tmp && (opcode & 0xff) == 0x9e) ||
            (tmp1 == tmp && (opcode & 0xff) == 0xa1))
        {
            addr = chip8->pc + 4;
            do_branch = true;
        }

        break;
    case 0x0f:
        switch (opcode & 0xff)
        {
        case 0x07:
            chip8->vs[(opcode & 0x0f00)>>8] = chip8->deley_timer++;
            break;
        case 0x0a:
            while (1)
            {
                SDL_PollEvent( &event );
                if (event.type == SDL_KEYDOWN)
                {
                    chip8->vs[(opcode & 0x0f00)>>8] = parse_int(event.key.keysym.sym);
                    break;
                }
                usleep(500);
            }
            break;
        case 0x15:
            chip8->deley_timer = chip8->vs[(opcode & 0x0f00)>>8];
            break;
        case 0x18:
            chip8->sound_timer = chip8->vs[(opcode & 0x0f00)>>8];
            break;
        case 0x1e:
            chip8->i += chip8->vs[(opcode & 0x0f00)>>8];
            break;
        case 0x29:
            chip8->i = chip8->vs[(opcode & 0x0f00)>>8]*5;
            break;
        case 0x33: break;
            chip8->mem[chip8->i] = chip8->vs[(opcode & 0x0f00)>>8] / 100;
            chip8->mem[chip8->i+1] = (chip8->vs[(opcode & 0x0f00)>>8] / 10) % 10;
            chip8->mem[chip8->i+2] = chip8->vs[(opcode & 0x0f00)>>8] % 10;
        case 0x55:
            for (uint8_t i=0;i<=(opcode & 0x0f00)>>8; i++)
            {
                chip8->mem[chip8->i + i] = chip8->vs[i];
            }
            break;
        case 0x65: break;
        }
        break;
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
    printf("pc = %x\ni = %.6x\n", chip8->pc, chip8->i);
    for (int i=0;i<16;i++)
    {
        printf("v[%i] = %x\n", i, chip8->vs[i]);
    }
}

int main(int argc, char** argv)
{
    cpu chip8;
    chip8.mem[0] = 0xF0;
    chip8.mem[1] = 0x90;
    chip8.mem[2] = 0x90;
    chip8.mem[3] = 0x90;
    chip8.mem[4] = 0xF0;
    // 1
    chip8.mem[5] = 0x20;
    chip8.mem[6] = 0x60;
    chip8.mem[7] = 0x20;
    chip8.mem[8] = 0x20;
    chip8.mem[9] = 0x70;
    // 2
    chip8.mem[10] = 0xF0;
    chip8.mem[11] = 0x10;
    chip8.mem[12] = 0xF0;
    chip8.mem[13] = 0x80;
    chip8.mem[14] = 0xF0;
    // 3
    chip8.mem[15] = 0xF0;
    chip8.mem[16] = 0x10;
    chip8.mem[17] = 0xF0;
    chip8.mem[18] = 0x10;
    chip8.mem[19] = 0xF0;
    // 4
    chip8.mem[20] = 0x90;
    chip8.mem[21] = 0x90;
    chip8.mem[22] = 0xF0;
    chip8.mem[23] = 0x10;
    chip8.mem[24] = 0x10;
    // 5
    chip8.mem[25] = 0xF0;
    chip8.mem[26] = 0x80;
    chip8.mem[27] = 0xF0;
    chip8.mem[28] = 0x10;
    chip8.mem[29] = 0xF0;
    // 6
    chip8.mem[30] = 0xF0;
    chip8.mem[31] = 0x80;
    chip8.mem[32] = 0xF0;
    chip8.mem[33] = 0x90;
    chip8.mem[34] = 0xF0;
    // 7
    chip8.mem[35] = 0xF0;
    chip8.mem[36] = 0x10;
    chip8.mem[37] = 0x20;
    chip8.mem[38] = 0x40;
    chip8.mem[39] = 0x40;
    // 8
    chip8.mem[40] = 0xF0;
    chip8.mem[41] = 0x90;
    chip8.mem[42] = 0xF0;
    chip8.mem[43] = 0x90;
    chip8.mem[44] = 0xF0;
    // 9
    chip8.mem[45] = 0xF0;
    chip8.mem[46] = 0x90;
    chip8.mem[47] = 0xF0;
    chip8.mem[48] = 0x10;
    chip8.mem[49] = 0xF0;
    // a
    chip8.mem[50] = 0xF0;
    chip8.mem[51] = 0x90;
    chip8.mem[52] = 0xF0;
    chip8.mem[53] = 0x90;
    chip8.mem[54] = 0x90;
    // b
    chip8.mem[55] = 0xE0;
    chip8.mem[56] = 0x90;
    chip8.mem[57] = 0xE0;
    chip8.mem[58] = 0x90;
    chip8.mem[59] = 0xE0;
    // c
    chip8.mem[60] = 0xF0;
    chip8.mem[61] = 0x80;
    chip8.mem[62] = 0x80;
    chip8.mem[63] = 0x80;
    chip8.mem[64] = 0xF0;
    // d
    chip8.mem[65] = 0xE0;
    chip8.mem[66] = 0x90;
    chip8.mem[67] = 0x90;
    chip8.mem[68] = 0x90;
    chip8.mem[69] = 0xE0;
    // e
    chip8.mem[70] = 0xF0;
    chip8.mem[71] = 0x80;
    chip8.mem[72] = 0xF0;
    chip8.mem[73] = 0x80;
    chip8.mem[74] = 0xF0;
    // f
    chip8.mem[75] = 0xF0;
    chip8.mem[76] = 0x80;
    chip8.mem[77] = 0xF0;
    chip8.mem[78] = 0x80;
    chip8.mem[79] = 0x80;


    chip8.pc = 0x0200;
    chip8.sp = 0;

    srand(time(NULL));
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_SetVideoMode(800,600,32,SDL_SWSURFACE);
    FILE *img = fopen(argv[1], "rb");
    size_t size;

    fseek(img, 0, SEEK_END);
    size = ftell(img);
    rewind(img);

    fread(chip8.mem + 0x0200, 1, size, img);
    fclose(img);

    char c;
    while(1)
    {
        step(&chip8);
        dump(&chip8);
        //getchar();
        // usleep(100000);
        SDL_Flip(window);
    }

    return 0;
}
