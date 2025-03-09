//
// Copyright (C) 1993-1996 Id Software, Inc.
// Copyright (C) 1993-2008 Raven Software
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
//  System interface for sound.
//

#include <stdio.h>

#include "dmx.h"

#include "i_ibm.h"
#include "i_system.h"
#include "s_sound.h"
#include "i_sound.h"
#include "sounds.h"
#include "m_misc.h"
#include "w_wad.h"
#include "z_zone.h"

#include "doomdef.h"
#include "doomstat.h"

#include "ns_task.h"
#include "ns_music.h"
#include "ns_cms.h"

#include "options.h"

#include "ns_cd.h"
#include "version.h"

//
// Sound header & data
//
int snd_Mport;   // midi variables
int snd_Sport;   // sound port
int snd_Rate;    // sound rate
int snd_PCMRate; // sound PCM rate

int snd_MusicVolume; // maximum volume for music
int snd_SfxVolume;   // maximum volume for sound

int snd_SfxDevice;   // current sfx card # (index to dmxCodes)
int snd_MusicDevice; // current music card # (index to dmxCodes)
int snd_MidiDevice;  // current midi device
int snd_DesiredSfxDevice;
int snd_DesiredMusicDevice;
int snd_DesiredMidiDevice;

//
// Retrieve the raw data lump index
//  for a given SFX name.
//
int I_GetSfxLumpNum(sfxinfo_t *sfx)
{
    if (snd_SfxDevice > snd_PC)
    {
        char namebuf[9] = "DS";
        strcpy(namebuf + 2, sfx->name);
        return W_GetNumForName(namebuf);
    }
    else
    {
        char namebuf[9] = "DP";
        strcpy(namebuf + 2, sfx->name);
        return W_GetNumForName(namebuf);
    }
}

//
// Sound startup stuff
//

void I_sndArbitrateCards(void)
{
    byte gus, adlib, adlibfx, sb, midi, ensoniq, lpt, cmsfx, cmsmus, oplxlptmus, oplxlptsnd, audiocd, rs232midi, lptmidi;
    int dmxlump;

    snd_SfxVolume = 127;
    snd_SfxDevice = snd_DesiredSfxDevice;
    snd_MusicDevice = snd_DesiredMusicDevice;
    snd_MidiDevice = snd_DesiredMidiDevice;

    //
    // check command-line parameters- overrides config file
    //
    if (M_CheckParm("-nosound"))
    {
        snd_MusicDevice = snd_SfxDevice = snd_none;
        return;
    }
    if (M_CheckParm("-nosfx"))
    {
        snd_SfxDevice = snd_none;
    }
    if (M_CheckParm("-nomusic"))
    {
        snd_MusicDevice = snd_none;
    }

    //
    // figure out what i've got to initialize
    //
    gus = snd_MusicDevice == snd_GUS || snd_SfxDevice == snd_GUS;
    sb = snd_SfxDevice == snd_SB || snd_SfxDevice == snd_SBDirect;
    ensoniq = snd_SfxDevice == snd_ENSONIQ;
    adlib = snd_MusicDevice == snd_Adlib || snd_MusicDevice == snd_SB || snd_MusicDevice == snd_PAS;
    oplxlptmus = snd_MusicDevice == snd_OPL2LPT || snd_MusicDevice == snd_OPL3LPT;
    oplxlptsnd = snd_SfxDevice == snd_OPL2LPT || snd_SfxDevice == snd_OPL3LPT;
    midi = snd_MusicDevice == snd_MPU;
    lpt = snd_SfxDevice == snd_DISNEY || snd_SfxDevice == snd_LPTDAC;
    cmsfx = snd_SfxDevice == snd_CMS;
    cmsmus = snd_MusicDevice == snd_CMS;
    audiocd = snd_MusicDevice == snd_CD;
    adlibfx = snd_SfxDevice == snd_Adlib;
    rs232midi = snd_MusicDevice == snd_RS232MIDI;
    lptmidi = snd_MusicDevice == snd_LPTMIDI;

    //
    // initialize whatever i've got
    //
    if (ensoniq)
    {
        if (ENS_Detect())
        {
            printf("ENSONIQ isn't responding\n");
        }
    }
    if (gus)
    {
        if (gamemode == commercial)
        {
            dmxlump = W_GetNumForName("DMXGUSC");
        }
        else
        {
            dmxlump = W_GetNumForName("DMXGUS");
        }
        GF1_SetMap(W_CacheLumpNum(dmxlump, PU_CACHE), lumpinfo[dmxlump].size);
    }
    if (sb)
    {
        SB_Detect();
    }

    if (adlib)
    {
        void *genmidi = W_CacheLumpName("GENMIDI", PU_STATIC);
        AL_SetCard(genmidi);
        Z_Free(genmidi);
    }

    if (oplxlptmus)
    {
        void *genmidi = W_CacheLumpName("GENMIDI", PU_STATIC);
        AL_SetCard(genmidi);
        Z_Free(genmidi);
        SetMUSPort(snd_Mport);
    }

    if (oplxlptsnd)
    {
        SetSNDPort(snd_Sport);
    }

    if (midi)
    {
        if (MPU_Detect(&snd_Mport))
        {
            printf("MPU-401 isn't reponding @ p=0x%x\n", snd_Mport);
        }
        else
        {
            SetMUSPort(snd_Mport);
        }
    }

    if (rs232midi)
    {
        SetMUSPort(snd_Mport);
    }

    if (lptmidi)
    {
        SetMUSPort(snd_Mport);
    }

    if (lpt)
    {
        SetSNDPort(snd_Sport);
    }

    if (cmsfx)
    {
        SetSNDPort(snd_Sport);
    }

    if (cmsmus)
    {
        SetMUSPort(snd_Mport);
    }

    if (adlibfx)
    {
        SetSNDPort(snd_Sport);
    }

    if (cmsfx && cmsmus)
    {
        CMS_SetMode(CMS_MusicFX);
    }
    else
    {
        if (cmsfx)
        {
            CMS_SetMode(CMS_OnlyFX);
        }

        if (cmsmus)
        {
            CMS_SetMode(CMS_OnlyMusic);
        }
    }

    if (audiocd)
    {
        if (!CD_Init())
        {
            // Error on AudioCD init
            I_Error("Cannot play AudioCD music");
        }

        CD_SetVolume(255);
        CD_Lock(LOCK);
    }
}

//
// I_StartupSound
// Inits all sound stuff
//
void I_StartupSound(void)
{

    //
    // pick the sound cards i'm going to use
    //
    I_sndArbitrateCards();

    //
    // inits ASS sound library
    //
    printf("  calling ASS_Init\n");

    ASS_Init(SND_TICRATE, snd_MusicDevice, snd_SfxDevice);

    switch(snd_MidiDevice)
    {
        case midi_mt32:
            printf("  loading MT-32 SysEx\n");
            MUS_TextMT32("Loading GM patches  ", 20);

            // Load MIDI
            MUS_LoadMT32();

            // Play MIDI
            MUS_ChainSong(0, -1);
            MUS_PlaySong(0, snd_MusicVolume);

            // Wait until is fully loaded
            while (MUS_SongPlaying())
            {
            }

            // Release MIDI
            MUS_ReleaseData();

            // Send text to MT-32
            MUS_TextMT32("## FastDOOM " FDOOMVERSION " ##", 20);

            delay(3000);
            break;
        case midi_sc55:
            MUS_ImgSC55();
            MUS_TextSC55("Version  " FDOOMVERSION, 14);
            delay(3000);
            break;
        case midi_mu80:
            MUS_YamahaXG();
            MUS_TextMU80("    FastDOOM          " FDOOMVERSION "     ", 32);
            break;
        case midi_tg300:
            MUS_ImgTG300();
            delay(3000);
            break;
        default:
            return;
    }
}
//
// I_ShutdownSound
// Shuts down all sound stuff
//
void I_ShutdownSound(void)
{
    S_PauseMusic();
    ASS_DeInit();
}
