//
// Copyright (C) 1993-1996 Id Software, Inc.
// Copyright (C) 2016-2017 Alexey Khokholov (Nuke.YKT)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Handle Sector base lighting effects.
//	Muzzle flash?
//

#include "doomstat.h"
#include "options.h"
#include "i_random.h"

#include "z_zone.h"
#include "m_misc.h"

#include "doomdef.h"
#include "p_local.h"

// State.
#include "r_state.h"

//
// FIRELIGHT FLICKER
//

//
// T_FireFlicker
//
void T_FireFlicker(fireflicker_t *flick)
{
    int amount;

    if (--flick->count)
        return;

    amount = (P_Random_And3_Mul16);

    if (flick->sector->lightlevel - amount < flick->minlight)
        flick->sector->lightlevel = flick->minlight;
    else
        flick->sector->lightlevel = flick->maxlight - amount;

    flick->count = 4;
}

//
// P_SpawnFireFlicker
//
void P_SpawnFireFlicker(sector_t *sector)
{
    fireflicker_t *flick;

    // Note that we are resetting sector attributes.
    // Nothing special about it during gameplay.
    sector->special = 0;

    flick = Z_MallocUnowned(sizeof(*flick), PU_LEVSPEC);

    thinkercap.prev->next = &flick->thinker;
    flick->thinker.next = &thinkercap;
    flick->thinker.prev = thinkercap.prev;
    thinkercap.prev = &flick->thinker;

    flick->thinker.function.acp1 = (actionf_p1)T_FireFlicker;
    flick->sector = sector;
    flick->maxlight = sector->lightlevel;
    flick->minlight = P_FindMinSurroundingLight(sector) + 16;
    flick->count = 4;
}

//
// BROKEN LIGHT FLASHING
//

//
// T_LightFlash
// Do flashing lights.
//
void T_LightFlash(lightflash_t *flash)
{
    if (--flash->count)
        return;

    if (flash->sector->lightlevel == flash->maxlight)
    {
        flash->sector->lightlevel = flash->minlight;
        flash->count = (P_Random & flash->mintime) + 1;
    }
    else
    {
        flash->sector->lightlevel = flash->maxlight;
        flash->count = (P_Random & flash->maxtime) + 1;
    }
}

//
// P_SpawnLightFlash
// After the map has been loaded, scan each sector
// for specials that spawn thinkers
//
void P_SpawnLightFlash(sector_t *sector)
{
    lightflash_t *flash;

    // nothing special about it during gameplay
    sector->special = 0;

    flash = Z_MallocUnowned(sizeof(*flash), PU_LEVSPEC);

    thinkercap.prev->next = &flash->thinker;
    flash->thinker.next = &thinkercap;
    flash->thinker.prev = thinkercap.prev;
    thinkercap.prev = &flash->thinker;

    flash->thinker.function.acp1 = (actionf_p1)T_LightFlash;
    flash->sector = sector;
    flash->maxlight = sector->lightlevel;

    flash->minlight = P_FindMinSurroundingLight(sector);
    flash->maxtime = 64;
    flash->mintime = 7;
    flash->count = (P_Random & flash->maxtime) + 1;
}

//
// STROBE LIGHT FLASHING
//

//
// T_StrobeFlash
//
void T_StrobeFlash(strobe_t *flash)
{
    if (--flash->count)
        return;

    if (flash->sector->lightlevel == flash->minlight)
    {
        flash->sector->lightlevel = flash->maxlight;
        flash->count = STROBEBRIGHT;
    }
    else
    {
        flash->sector->lightlevel = flash->minlight;
        flash->count = flash->darktime;
    }
}

//
// P_SpawnStrobeFlash
// After the map has been loaded, scan each sector
// for specials that spawn thinkers
//
void P_SpawnStrobeFlash(sector_t *sector,
                        int fastOrSlow,
                        int inSync)
{
    strobe_t *flash;

    flash = Z_MallocUnowned(sizeof(*flash), PU_LEVSPEC);

    thinkercap.prev->next = &flash->thinker;
    flash->thinker.next = &thinkercap;
    flash->thinker.prev = thinkercap.prev;
    thinkercap.prev = &flash->thinker;

    flash->sector = sector;
    flash->darktime = fastOrSlow;
    flash->thinker.function.acp1 = (actionf_p1)T_StrobeFlash;
    flash->maxlight = sector->lightlevel;
    flash->minlight = P_FindMinSurroundingLight(sector);

    if (flash->minlight == flash->maxlight)
        flash->minlight = 0;

    // nothing special about it during gameplay
    sector->special = 0;

    if (!inSync)
        flash->count = P_Random_And7_Plus1;
    else
        flash->count = 1;
}

//
// Start strobing lights (usually from a trigger)
//
void EV_StartLightStrobing(line_t *line)
{
    int secnum;
    sector_t *sec;

    secnum = -1;
    while ((secnum = P_FindSectorFromLineTag(line->tag, secnum)) >= 0)
    {
        sec = &sectors[secnum];
        if (sec->specialdata)
            continue;

        P_SpawnStrobeFlash(sec, SLOWDARK, 0);
    }
}

//
// TURN LINE'S TAG LIGHTS OFF
//
void EV_TurnTagLightsOff(line_t *line)
{
    int j;

    // search sectors for those with same tag as activating line

    // killough 10/98: replaced inefficient search with fast search
    for (j = -1; (j = P_FindSectorFromLineTag(line->tag, j)) >= 0;)
    {
        sector_t *sector = sectors + j, *tsec;
        short i; 
        short min = sector->lightlevel;
        // find min neighbor light level
        for (i = 0; i < sector->linecount; i++)
            if ((tsec = getNextSector(sector->lines[i], sector)) && tsec->lightlevel < min)
                min = tsec->lightlevel;
        sector->lightlevel = min;
    }
}
//
// TURN LINE'S TAG LIGHTS ON
//
void EV_LightTurnOn(line_t *line, int bright)
{
    int i;

    // search all sectors for ones with same tag as activating line

    // killough 10/98: replace inefficient search with fast search
    for (i = -1; (i = P_FindSectorFromLineTag(line->tag, i)) >= 0;)
    {
        sector_t *temp, *sector = sectors + i;
        short j; 
        int tbright = bright; //jff 5/17/98 search for maximum PER sector

        // bright = 0 means to search for highest light level surrounding sector

        if (!bright)
            for (j = 0; j < sector->linecount; j++)
                if ((temp = getNextSector(sector->lines[j], sector)) && temp->lightlevel > tbright)
                    tbright = temp->lightlevel;

        sector->lightlevel = tbright;
        bright = tbright;
    }
}

//
// Spawn glowing light
//

void T_Glow(glow_t *g)
{
    switch (g->direction)
    {
    case -1:
        // DOWN
        g->sector->lightlevel -= GLOWSPEED;
        if (g->sector->lightlevel <= g->minlight)
        {
            g->sector->lightlevel += GLOWSPEED;
            g->direction = 1;
        }
        break;

    case 1:
        // UP
        g->sector->lightlevel += GLOWSPEED;
        if (g->sector->lightlevel >= g->maxlight)
        {
            g->sector->lightlevel -= GLOWSPEED;
            g->direction = -1;
        }
        break;
    }
}

void P_SpawnGlowingLight(sector_t *sector)
{
    glow_t *g;

    g = Z_MallocUnowned(sizeof(*g), PU_LEVSPEC);

    thinkercap.prev->next = &g->thinker;
    g->thinker.next = &thinkercap;
    g->thinker.prev = thinkercap.prev;
    thinkercap.prev = &g->thinker;

    g->sector = sector;
    g->minlight = P_FindMinSurroundingLight(sector);
    g->maxlight = sector->lightlevel;
    g->thinker.function.acp1 = (actionf_p1)T_Glow;
    g->direction = -1;

    sector->special = 0;
}
