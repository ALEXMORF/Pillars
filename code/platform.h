#include <stdio.h>

struct screen_buffer
{
    u32 *Data;
    int Width;
    int Height;
};

struct input
{
    b32 Left;
    b32 Right;
    b32 Up;
    b32 Down;
    
    int MouseDX;
    int MouseDY;
};
