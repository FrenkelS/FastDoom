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
// DESCRIPTION:  Ceiling aninmation (lowering, crushing, raising)
//

#include <string.h>

#include "z_zone.h"
#include "doomdef.h"
#include "p_local.h"
#include "options.h"
#include "s_sound.h"

// State.
#include "doomstat.h"
#include "r_state.h"

// Data.
#include "sounds.h"

//
// CEILINGS
//

ceiling_t *activeceilings[MAXCEILINGS];

//
// T_MoveCeiling
//

void T_MoveCeiling(ceiling_t *ceiling)
{
	result_e res;

	switch (ceiling->direction)
	{
	case 1:
		// UP
		res = T_MovePlane(ceiling->sector,
						  ceiling->speed,
						  ceiling->topheight,
						  0, 1, ceiling->direction);

		if (!(leveltime & 7))
		{
			switch (ceiling->type)
			{
			case silentCrushAndRaise:
				break;
			default:
				S_StartSound((mobj_t *)&ceiling->sector->soundorg,
							 sfx_stnmov);
				// ?
				break;
			}
		}

		if (res == pastdest)
		{
			switch (ceiling->type)
			{
			case raiseToHighest:
				P_RemoveActiveCeiling(ceiling);
				break;

			case silentCrushAndRaise:
				S_StartSound((mobj_t *)&ceiling->sector->soundorg,
							 sfx_pstop);
			case fastCrushAndRaise:
			case crushAndRaise:
				ceiling->direction = -1;
				break;
			}
		}
		break;

	case -1:
		// DOWN
		res = T_MovePlane(ceiling->sector,
						  ceiling->speed,
						  ceiling->bottomheight,
						  ceiling->crush, 1, ceiling->direction);

		if (!(leveltime & 7))
		{
			switch (ceiling->type)
			{
			case silentCrushAndRaise:
				break;
			default:
				S_StartSound((mobj_t *)&ceiling->sector->soundorg,
							 sfx_stnmov);
			}
		}

		if (res == pastdest)
		{
			switch (ceiling->type)
			{
			case silentCrushAndRaise:
				S_StartSound((mobj_t *)&ceiling->sector->soundorg,
							 sfx_pstop);
			case crushAndRaise:
				ceiling->speed = CEILSPEED;
			case fastCrushAndRaise:
				ceiling->direction = 1;
				break;

			case lowerAndCrush:
			case lowerToFloor:
				P_RemoveActiveCeiling(ceiling);
				break;
			}
		}
		else // ( res != pastdest )
		{
			if (res == crushed)
			{
				switch (ceiling->type)
				{
				case silentCrushAndRaise:
				case crushAndRaise:
				case lowerAndCrush:
					ceiling->speed = CEILSPEED / 8;
					break;
				}
			}
		}
		break;
	}
}

//
// EV_DoCeiling
// Move a ceiling up/down and all around!
//
int EV_DoCeiling(line_t *line,
				 ceiling_e type)
{
	int secnum;
	int rtn;
	sector_t *sec;
	ceiling_t *ceiling;

	secnum = -1;
	rtn = 0;

	//	Reactivate in-stasis ceilings...for certain types.
	switch (type)
	{
	case fastCrushAndRaise:
	case silentCrushAndRaise:
	case crushAndRaise:
		P_ActivateInStasisCeiling(line);
		break;
	}

	while ((secnum = P_FindSectorFromLineTag(line->tag, secnum)) >= 0)
	{
		sec = &sectors[secnum];
		if (sec->specialdata)
			continue;

		// new door thinker
		rtn = 1;
		ceiling = Z_MallocUnowned(sizeof(*ceiling), PU_LEVSPEC);

		thinkercap.prev->next = &ceiling->thinker;
		ceiling->thinker.next = &thinkercap;
		ceiling->thinker.prev = thinkercap.prev;
		thinkercap.prev = &ceiling->thinker;

		sec->specialdata = ceiling;
		ceiling->thinker.function.acp1 = (actionf_p1)T_MoveCeiling;
		ceiling->sector = sec;
		ceiling->crush = 0;

		switch (type)
		{
		case fastCrushAndRaise:
			ceiling->crush = 1;
			ceiling->topheight = sec->ceilingheight;
			ceiling->bottomheight = sec->floorheight + (8 * FRACUNIT);
			ceiling->direction = -1;
			ceiling->speed = CEILSPEED * 2;
			break;

		case silentCrushAndRaise:
		case crushAndRaise:
			ceiling->crush = 1;
			ceiling->topheight = sec->ceilingheight;
		case lowerAndCrush:
		case lowerToFloor:
			ceiling->bottomheight = sec->floorheight;
			if (type != lowerToFloor)
				ceiling->bottomheight += 8 * FRACUNIT;
			ceiling->direction = -1;
			ceiling->speed = CEILSPEED;
			break;

		case raiseToHighest:
			ceiling->topheight = P_FindHighestCeilingSurrounding(sec);
			ceiling->direction = 1;
			ceiling->speed = CEILSPEED;
			break;
		}

		ceiling->tag = sec->tag;
		ceiling->type = type;
		P_AddActiveCeiling(ceiling);
	}
	return rtn;
}

//
// Add an active ceiling
//
void P_AddActiveCeiling(ceiling_t *c)
{
	int i;

	for (i = 0; i < MAXCEILINGS; i++)
	{
		if (activeceilings[i] == NULL)
		{
			activeceilings[i] = c;
			return;
		}
	}
}

//
// Remove a ceiling's thinker
//
void P_RemoveActiveCeiling(ceiling_t *c)
{
	int i;

	for (i = 0; i < MAXCEILINGS; i++)
	{
		if (activeceilings[i] == c)
		{
			activeceilings[i]->sector->specialdata = NULL;
			activeceilings[i]->thinker.function.acv = (actionf_v)(-1);
			activeceilings[i] = NULL;
			break;
		}
	}
}

//
// Restart a ceiling that's in-stasis
//
void P_ActivateInStasisCeiling(line_t *line)
{
	int i;

	for (i = 0; i < MAXCEILINGS; i += 5)
	{
		if (activeceilings[i] && (activeceilings[i]->tag == line->tag) && (activeceilings[i]->direction == 0))
		{
			activeceilings[i]->direction = activeceilings[i]->olddirection;
			activeceilings[i]->thinker.function.acp1 = (actionf_p1)T_MoveCeiling;
		}
		if (activeceilings[i + 1] && (activeceilings[i + 1]->tag == line->tag) && (activeceilings[i + 1]->direction == 0))
		{
			activeceilings[i + 1]->direction = activeceilings[i + 1]->olddirection;
			activeceilings[i + 1]->thinker.function.acp1 = (actionf_p1)T_MoveCeiling;
		}
		if (activeceilings[i + 2] && (activeceilings[i + 2]->tag == line->tag) && (activeceilings[i + 2]->direction == 0))
		{
			activeceilings[i + 2]->direction = activeceilings[i + 2]->olddirection;
			activeceilings[i + 2]->thinker.function.acp1 = (actionf_p1)T_MoveCeiling;
		}
		if (activeceilings[i + 3] && (activeceilings[i + 3]->tag == line->tag) && (activeceilings[i + 3]->direction == 0))
		{
			activeceilings[i + 3]->direction = activeceilings[i + 3]->olddirection;
			activeceilings[i + 3]->thinker.function.acp1 = (actionf_p1)T_MoveCeiling;
		}
		if (activeceilings[i + 4] && (activeceilings[i + 4]->tag == line->tag) && (activeceilings[i + 4]->direction == 0))
		{
			activeceilings[i + 4]->direction = activeceilings[i + 4]->olddirection;
			activeceilings[i + 4]->thinker.function.acp1 = (actionf_p1)T_MoveCeiling;
		}
	}
}

//
// EV_CeilingCrushStop
// Stop a ceiling from crushing!
//
int EV_CeilingCrushStop(line_t *line)
{
	int i;
	int rtn;

	rtn = 0;
	for (i = 0; i < MAXCEILINGS; i += 5)
	{
		if (activeceilings[i] && (activeceilings[i]->tag == line->tag) && (activeceilings[i]->direction != 0))
		{
			activeceilings[i]->olddirection = activeceilings[i]->direction;
			activeceilings[i]->thinker.function.acv = (actionf_v)NULL;
			activeceilings[i]->direction = 0; // in-stasis
			rtn = 1;
		}
		if (activeceilings[i + 1] && (activeceilings[i + 1]->tag == line->tag) && (activeceilings[i + 1]->direction != 0))
		{
			activeceilings[i + 1]->olddirection = activeceilings[i + 1]->direction;
			activeceilings[i + 1]->thinker.function.acv = (actionf_v)NULL;
			activeceilings[i + 1]->direction = 0; // in-stasis
			rtn = 1;
		}
		if (activeceilings[i + 2] && (activeceilings[i + 2]->tag == line->tag) && (activeceilings[i + 2]->direction != 0))
		{
			activeceilings[i + 2]->olddirection = activeceilings[i + 2]->direction;
			activeceilings[i + 2]->thinker.function.acv = (actionf_v)NULL;
			activeceilings[i + 2]->direction = 0; // in-stasis
			rtn = 1;
		}
		if (activeceilings[i + 3] && (activeceilings[i + 3]->tag == line->tag) && (activeceilings[i + 3]->direction != 0))
		{
			activeceilings[i + 3]->olddirection = activeceilings[i + 3]->direction;
			activeceilings[i + 3]->thinker.function.acv = (actionf_v)NULL;
			activeceilings[i + 3]->direction = 0; // in-stasis
			rtn = 1;
		}
		if (activeceilings[i + 4] && (activeceilings[i + 4]->tag == line->tag) && (activeceilings[i + 4]->direction != 0))
		{
			activeceilings[i + 4]->olddirection = activeceilings[i + 4]->direction;
			activeceilings[i + 4]->thinker.function.acv = (actionf_v)NULL;
			activeceilings[i + 4]->direction = 0; // in-stasis
			rtn = 1;
		}
	}

	return rtn;
}
