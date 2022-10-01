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
//   All the global variables that store the internal state.
//   Theoretically speaking, the internal state of the engine
//    should be found by looking at the variables collected
//    here, and every relevant module will have to include
//    this header file.
//   In practice, things are a bit messy.
//

#ifndef __D_STATE__
#define __D_STATE__

// We need globally shared data structures,
//  for defining the global state variables.
#include "doomdata.h"
#include "d_net.h"

// We need the playr data structure as well.
#include "d_player.h"

#include "options.h"



// ------------------------
// Command line parameters.
//
extern boolean nomonsters;  // checkparm of -nomonsters
extern boolean respawnparm; // checkparm of -respawn
extern boolean fastparm;    // checkparm of -fast

extern boolean flatSurfaces;
extern boolean untexturedSurfaces;
extern boolean flatSky;
extern boolean flatShadows;
extern boolean saturnShadows;
extern boolean showFPS;
extern boolean unlimitedRAM;
extern boolean nearSprites;
extern boolean monoSound;
extern boolean lowSound;
extern boolean noMelt;

extern boolean reverseStereo;

extern boolean forceHighDetail;
extern boolean forceLowDetail;
extern boolean forcePotatoDetail;
extern int forceScreenSize;

#if defined(MODE_T8025) || defined(MODE_T4025) || defined(MODE_T4050)
extern boolean CGAcard;
#endif

#if defined(MODE_CGA)
extern boolean CGApalette1;
#endif

#if defined(MODE_CGA16) || defined(MODE_CGA136)
extern boolean CGAfix;
#endif

#ifdef SUPPORTS_HERCULES_AUTOMAP
extern boolean HERCmap;
#endif

#if defined(MODE_Y) || defined(MODE_13H) || defined(MODE_VBE2) || defined(MODE_VBE2_DIRECT) || defined(MODE_V2)
extern boolean VGADACfix;
#endif

#if defined(MODE_T8050) || defined(MODE_T80100)
extern boolean videoPageFix;
#endif

extern boolean logTimedemo;
extern boolean disableDemo;

extern boolean uncappedFPS;
extern boolean waitVsync;
extern boolean simpleStatusBar;
extern boolean debugPort;

// Set if homebrew PWAD stuff has been added.
extern boolean modifiedgame;

extern unsigned char complevel;

// -------------------------------------------
// Selected skill type, map etc.
//

// Defaults for menu, methinks.
extern skill_t startskill;
extern int startepisode;
extern int startmap;

// Selected by user.
extern skill_t gameskill;
extern int gameepisode;
extern int gamemap;

extern boolean bfgedition;
extern gamemode_t gamemode;
extern gamemission_t gamemission;

// Nightmare mode flag, single player.
extern boolean respawnmonsters;

// Flag: true only if started as net deathmatch.
// An enum might handle altdeath/cooperative better.
extern boolean deathmatch;

// -------------------------
// Internal parameters for sound rendering.
// These have been taken from the DOS version,
//  but are not (yet) supported with Linux
//  (e.g. no sound volume adjustment with menu.

// These are not used, but should be (menu).
// From m_menu.c:
//  Sound FX volume has default, 0 - 15
//  Music volume has default, 0 - 15
// These are multiplied by 8.
//extern int snd_SfxVolume;      // maximum volume for sound
//extern int snd_MusicVolume;    // maximum volume for music

// Current music/sfx card - index useless
//  w/o a reference LUT in a sound module.
// Ideally, this would use indices found
//  in: /usr/include/linux/soundcard.h
extern int snd_MusicDevice;
extern int snd_SfxDevice;
// Config file? Same disclaimer as above.
extern int snd_DesiredMusicDevice;
extern int snd_DesiredSfxDevice;

// -------------------------
// Status flags for refresh.
//

// Depending on view size - no status bar?
// Note that there is no way to disable the
//  status bar explicitely.
extern boolean statusbaractive;

extern byte automapactive; // In AutoMap mode?
extern byte menuactive;    // Menu overlayed?
extern byte paused;        // Game Pause?

extern byte viewactive;

extern struct ev_s *current_ev;

extern int viewwindowx;
extern int viewwindowy;
extern int viewheight;
extern int viewheightshift;
extern int viewheightopt;
extern int viewheight32;
extern int viewwidth;
extern int viewwidthhalf;
extern int viewwidthlimit;
extern int automapheight;
extern int scaledviewwidth;

#if defined(MODE_13H) || defined(MODE_VBE2)
extern int endscreen;
extern int startscreen;
#endif

// -------------------------------------
// Scores, rating.
// Statistics on a given map, for intermission.
//
extern int totalkills;
extern int totalitems;
extern int totalsecret;

// Timer, for scores.
extern int leveltime;     // tics in game play for par

// --------------------------------------
// DEMO playback/recording related stuff.
// No demo, there is a human player in charge?
// Disable save/end game?
extern byte usergame;

//?
extern byte demoplayback;
extern byte demorecording;
extern byte timingdemo;

// Quit after playing a demo from cmdline.
extern byte singledemo;

//?
extern gamestate_t gamestate;

//-----------------------------
// Internal parameters, fixed.
// These are set by the engine, and not changed
//  according to user inputs. Partly load from
//  WAD, partly set at startup time.

extern int gametic;

// Bookkeeping on players - state.
extern player_t players;
extern mobj_t *players_mo;

// Alive? Disconnected?
extern boolean playeringame;

// Intermission stats.
// Parameters for world map / intermission.
extern wbstartstruct_t wminfo;

// LUT of ammunition limits for each kind.
// This doubles with BackPack powerup item.
extern int maxammo[NUMAMMO];

//-----------------------------------------
// Internal parameters, used for engine.
//

// File handling stuff.
extern char basedefault[12];

// wipegamestate can be set to -1
//  to force a wipe on the next draw
extern gamestate_t wipegamestate;

extern int mouseSensitivity;
//?
// debug flag to cancel adaptiveness
extern boolean singletics;

// Needed to store the number of the dummy sky flat.
// Used for rendering,
//  as well as tracking projectiles etc.
extern short skyflatnum;

extern ticcmd_t localcmds[BACKUPTICS];

extern int maketic;

extern int autorun;

extern char *mapnames[];
extern char *mapnames2[];
extern char *mapnamesp[];
extern char *mapnamest[];

#endif
