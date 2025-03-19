#include <string.h>
#include <dos.h>
#include <conio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "doomtype.h"
#include "i_ibm.h"
#include "v_video.h"
#include "tables.h"
#include "math.h"
#include "i_system.h"
#include "doomstat.h"
#include "m_menu.h"

#if defined(MODE_MDA)

unsigned short backbuffer[80 * 25];
unsigned short *textdestscreen = backbuffer;

void MDA_InitGraphics(void)
{
    union REGS regs;

    // Set 80x25 monochrome mode
    regs.h.ah = 0x00;
    regs.h.al = 0x07;
    int386(0x10, &regs, &regs);

    // Disable cursor
    regs.h.ah = 0x01;
    regs.h.ch = 0x3F;
    int386(0x10, &regs, &regs);

    // Disable MDA blink
    I_DisableMDABlink();
}

void I_ProcessPalette(byte *palette)
{
    // Do nothing
}

void I_SetPalette(int numpalette)
{
    // Do nothing
}

void I_FinishUpdate(void)
{
    CopyDWords(backbuffer, 0xB0000, 1000);
}

#endif
