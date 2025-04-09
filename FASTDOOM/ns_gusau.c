#include <stdlib.h>
#include <conio.h>
#include <dos.h>
#include <string.h>
#include "ns_inter.h"
#include "ns_llm.h"
#include "ns_multi.h"
#include "ns_gusdf.h"
#include "ns_gf1.h"
#include "ns_gusmi.h"
#include "ns_gusau.h"
#include "options.h"
#include "z_zone.h"

static const char GUSWAVE_PanTable[32] =
    {
        8, 9, 10, 11, 11, 12, 13, 14,
        15, 14, 13, 12, 11, 10, 9, 8,
        7, 6, 5, 4, 4, 3, 2, 1,
        0, 1, 2, 3, 4, 5, 6, 7};

static voicelist VoiceList;
static voicelist VoicePool;

static voicestatus VoiceStatus[MAX_VOICES];
//static
VoiceNode GUSWAVE_Voices[VOICES];

static int GUSWAVE_VoiceHandle = GUSWAVE_MinVoiceHandle;
static int GUSWAVE_MaxVoices = VOICES;
//static
int GUSWAVE_Installed = FALSE;

// current volume for dig audio - from 0 to 4095
static int GUSWAVE_Volume = MAX_VOLUME;

static int GUSWAVE_SwapLeftRight = FALSE;

extern int GUSMIDI_Installed;

/*---------------------------------------------------------------------
   Function: GUSWAVE_CallBack

   GF1 callback service routine.
---------------------------------------------------------------------*/

unsigned char *GUS_Silence8;
unsigned short *GUS_Silence16;

static int LOADDS GUSWAVE_CallBack(
    int reason,
    int voice,
    unsigned char **buf,
    unsigned long *size)

{
    VoiceNode *Voice;
    playbackstatus status;

    // this function is called from an interrupt
    // remember not to make any DOS or BIOS calls from here
    // also don't call any C library functions unless you are sure that
    // they are reentrant
    // restore our DS register

    if (VoiceStatus[voice].playing == FALSE)
    {
        return (DIG_DONE);
    }

    if (reason == DIG_MORE_DATA)
    {
        Voice = VoiceStatus[voice].Voice;

        if ((Voice != NULL) && (Voice->Playing))
        {
            status = Voice->GetSound(Voice);
            if (status != SoundDone)
            {
                if ((Voice->sound == NULL) || (status == NoMoreData))
                {
                    if (Voice->bits == 8)
                    {
                        *buf = GUS_Silence8;
                    }
                    else
                    {
                        *buf = (unsigned char *)GUS_Silence16;
                    }
                    *size = 256;
                }
                else
                {
                    *buf = Voice->sound;
                    *size = Voice->length;
                }
                return (DIG_MORE_DATA);
            }
        }

        return (DIG_DONE);
    }

    if (reason == DIG_DONE)
    {
        Voice = VoiceStatus[voice].Voice;
        VoiceStatus[voice].playing = FALSE;

        if (Voice != NULL)
        {
            Voice->Active = FALSE;
            Voice->Playing = FALSE;

            // I'm commenting this out because a -1 could cause a crash if it
            // is sent to the GF1 code.  This shouldn't be necessary since
            // Active should be false when GF1voice is -1, but this is just
            // a precaution.  Adjust the pan on the wrong voice is a lot
            // more pleasant than a crash!
            //         Voice->GF1voice = -1;

            LL_Remove(VoiceNode, &VoiceList, Voice);
            LL_AddToTail(VoiceNode, &VoicePool, Voice);
        }

    }

    return (DIG_DONE);
}

/*---------------------------------------------------------------------
   Function: GUSWAVE_GetVoice

   Locates the voice with the specified handle.
---------------------------------------------------------------------*/

static VoiceNode *GUSWAVE_GetVoice(
    int handle)

{
    VoiceNode *voice;
    unsigned flags;

    flags = DisableInterrupts();

    voice = VoiceList.start;

    while (voice != NULL)
    {
        if (handle == voice->handle)
        {
            break;
        }

        voice = voice->next;
    }

    RestoreInterrupts(flags);

    return (voice);
}

/*---------------------------------------------------------------------
   Function: GUSWAVE_VoicePlaying

   Checks if the voice associated with the specified handle is
   playing.
---------------------------------------------------------------------*/

int GUSWAVE_VoicePlaying(
    int handle)

{
    VoiceNode *voice;

    voice = GUSWAVE_GetVoice(handle);
    if (voice != NULL)
    {
        return (voice->Active);
    }

    return (FALSE);
}

/*---------------------------------------------------------------------
   Function: GUSWAVE_VoicesPlaying

   Determines the number of currently active voices.
---------------------------------------------------------------------*/

int GUSWAVE_VoicesPlaying(
    void)

{
    int index;
    int NumVoices = 0;
    unsigned flags;

    flags = DisableInterrupts();

    for (index = 0; index < GUSWAVE_MaxVoices; index++)
    {
        if (GUSWAVE_Voices[index].Active)
        {
            NumVoices++;
        }
    }

    RestoreInterrupts(flags);

    return (NumVoices);
}

/*---------------------------------------------------------------------
   Function: GUSWAVE_Kill

   Stops output of the voice associated with the specified handle.
---------------------------------------------------------------------*/

int GUSWAVE_Kill(
    int handle)

{
    VoiceNode *voice;
    unsigned flags;

    flags = DisableInterrupts();

    voice = GUSWAVE_GetVoice(handle);

    if (voice == NULL)
    {
        RestoreInterrupts(flags);

        return (GUSWAVE_Warning);
    }

    RestoreInterrupts(flags);

    if (voice->Active)
    {
        gf1_stop_digital(voice->GF1voice);
    }

    //   RestoreInterrupts( flags );

    return (GUSWAVE_Ok);
}

/*---------------------------------------------------------------------
   Function: GUSWAVE_KillAllVoices

   Stops output of all currently active voices.
---------------------------------------------------------------------*/

int GUSWAVE_KillAllVoices(
    void)

{
    int i;
    unsigned flags;

    if (!GUSWAVE_Installed)
    {
        return (GUSWAVE_Ok);
    }

    flags = DisableInterrupts();

    // Remove all the voices from the list
    for (i = 0; i < GUSWAVE_MaxVoices; i++)
    {
        if (GUSWAVE_Voices[i].Active)
        {
            //         GUSWAVE_Kill( GUSWAVE_Voices[ i ].handle );

            gf1_stop_digital(GUSWAVE_Voices[i].GF1voice);
        }
    }

    for (i = 0; i < MAX_VOICES; i++)
    {
        VoiceStatus[i].playing = FALSE;
        VoiceStatus[i].Voice = NULL;
    }

    VoicePool.start = NULL;
    VoicePool.end = NULL;
    VoiceList.start = NULL;
    VoiceList.end = NULL;

    for (i = 0; i < GUSWAVE_MaxVoices; i++)
    {
        GUSWAVE_Voices[i].Active = FALSE;
        if (GUSWAVE_Voices[i].mem != 0) // Compare to NULL
        {
            LL_AddToTail(VoiceNode, &VoicePool, &GUSWAVE_Voices[i]);
        }
    }

    RestoreInterrupts(flags);

    return (GUSWAVE_Ok);
}

/*---------------------------------------------------------------------
   Function: GUSWAVE_SetVolume

   Sets the total volume of the digitized sounds.
---------------------------------------------------------------------*/

void GUSWAVE_SetVolume(
    int volume)

{
    int i;

    volume = max(0, volume);
    volume = min(255, volume);
    GUSWAVE_Volume = MAX_VOLUME - (255 - volume) * 4;

    for (i = 0; i < GUSWAVE_MaxVoices; i++)
    {
        if (GUSWAVE_Voices[i].Active)
        {
            gf1_dig_set_vol(GUSWAVE_Voices[i].GF1voice,
                            GUSWAVE_Volume - (255 - GUSWAVE_Voices[i].Volume) * 4);
        }
    }
}

/*---------------------------------------------------------------------
   Function: GUSWAVE_AllocVoice

   Retrieve an inactive or lower priority voice for output.
---------------------------------------------------------------------*/

static VoiceNode *GUSWAVE_AllocVoice(void)

{
    VoiceNode *voice;
    VoiceNode *node;
    unsigned flags;

    // If we don't have any free voices, check if we have a higher
    // priority than one that is playing.
    if (GUSWAVE_VoicesPlaying() >= GUSWAVE_MaxVoices)
    {
        flags = DisableInterrupts();

        node = VoiceList.start;
        voice = node;
        while (node != NULL)
        {
            if (node->priority < voice->priority)
            {
                voice = node;
            }

            node = node->next;
        }

        RestoreInterrupts(flags);

        if (0xffff >= voice->priority)
        {
            GUSWAVE_Kill(voice->handle);
        }
    }

    // Check if any voices are in the voice pool
    flags = DisableInterrupts();

    voice = VoicePool.start;
    if (voice != NULL)
    {
        LL_Remove(VoiceNode, &VoicePool, voice);
    }

    RestoreInterrupts(flags);

    if (voice != NULL)
    {
        do
        {
            GUSWAVE_VoiceHandle++;
            if (GUSWAVE_VoiceHandle < GUSWAVE_MinVoiceHandle)
            {
                GUSWAVE_VoiceHandle = GUSWAVE_MinVoiceHandle;
            }
        } while (GUSWAVE_VoicePlaying(GUSWAVE_VoiceHandle));

        voice->handle = GUSWAVE_VoiceHandle;
    }

    return (voice);
}

/*---------------------------------------------------------------------
   Function: GUSWAVE_GetNextDemandFeedBlock

   Controls playback of demand fed data.
---------------------------------------------------------------------*/

playbackstatus GUSWAVE_GetNextDemandFeedBlock(
    VoiceNode *voice)

{
    if (voice->BlockLength > 0)
    {
        voice->sound += voice->length;
        voice->length = min(voice->BlockLength, 0x8000);
        voice->BlockLength -= voice->length;

        return (KeepPlaying);
    }

    if (voice->DemandFeed == NULL)
    {
        return (SoundDone);
    }

    (voice->DemandFeed)(&voice->sound, &voice->BlockLength);
    //   voice->sound = GUS_Silence16;
    //   voice->BlockLength = 256;

    voice->length = min(voice->BlockLength, 0x8000);
    voice->BlockLength -= voice->length;

    if ((voice->length > 0) && (voice->sound != NULL))
    {
        return (KeepPlaying);
    }
    return (NoMoreData);
}

/*---------------------------------------------------------------------
   Function: GUSWAVE_Play

   Begins playback of digitized sound.
---------------------------------------------------------------------*/

int GUSWAVE_Play(
    VoiceNode *voice,
    int angle)

{
    int VoiceNumber;
    int type;
    int pan;
    unsigned flags;
    int (*servicefunction)(int reason, int voice, unsigned char **buf, unsigned long *size);

    type = 0;

    if (voice->bits == 8)
    {
        type |= TYPE_8BIT;
        type |= TYPE_INVERT_MSB;
    }

    voice->GF1voice = -1;

    angle &= 31;
    pan = GUSWAVE_PanTable[angle];
    if (GUSWAVE_SwapLeftRight)
    {
        pan = 15 - pan;
    }

    voice->Pan = pan;

    voice->Volume = 255;

    servicefunction = GUSWAVE_CallBack;

    VoiceNumber = gf1_play_digital(0, voice->sound, voice->length,
                                   voice->mem, GUSWAVE_Volume, pan,
                                   voice->RateScale, type, &GUS_HoldBuffer, servicefunction);

    if (VoiceNumber == NO_MORE_VOICES)
    {
        flags = DisableInterrupts();
        LL_AddToTail(VoiceNode, &VoicePool, voice);
        RestoreInterrupts(flags);

        return (GUSWAVE_Warning);
    }

    flags = DisableInterrupts();
    voice->GF1voice = VoiceNumber;
    voice->Active = TRUE;
    LL_AddToTail(VoiceNode, &VoiceList, voice);
    VoiceStatus[VoiceNumber].playing = TRUE;
    VoiceStatus[VoiceNumber].Voice = voice;

    RestoreInterrupts(flags);

    return (voice->handle);
}

/*---------------------------------------------------------------------
   Function: GUSWAVE_StartDemandFeedPlayback

   Begins playback of digitized sound.
---------------------------------------------------------------------*/

int GUSWAVE_StartDemandFeedPlayback(
    void (*function)(char **ptr, unsigned long *length),
    int bits,
    int rate,
    int angle)

{
    VoiceNode *voice;
    int handle;

    // Request a voice from the voice pool
    voice = GUSWAVE_AllocVoice();
    if (voice == NULL)
    {
        return (GUSWAVE_Warning);
    }

    voice->bits = bits;
    voice->GetSound = GUSWAVE_GetNextDemandFeedBlock;
    voice->Playing = TRUE;
    voice->DemandFeed = function;
    voice->BlockLength = 0;
    voice->length = 256;
    voice->sound = (bits == 8) ? GUS_Silence8 : (char *)GUS_Silence16;
    voice->NextBlock = NULL;
    voice->next = NULL;
    voice->prev = NULL;
    voice->priority = 0xffff;
    voice->RateScale = rate;

    handle = GUSWAVE_Play(voice, angle);

    return (handle);
}

/*---------------------------------------------------------------------
   Function: GUSWAVE_SetReverseStereo

   Set the orientation of the left and right channels.
---------------------------------------------------------------------*/

void GUSWAVE_SetReverseStereo(
    int setting)

{
    GUSWAVE_SwapLeftRight = setting;
}

/*---------------------------------------------------------------------
   Function: GUSWAVE_InitVoices

   Begins playback of digitized sound.
---------------------------------------------------------------------*/

static int GUSWAVE_InitVoices(void)
{
    int i;

    for (i = 0; i < MAX_VOICES; i++)
    {
        VoiceStatus[i].playing = FALSE;
        VoiceStatus[i].Voice = NULL;
    }

    VoicePool.start = NULL;
    VoicePool.end = NULL;
    VoiceList.start = NULL;
    VoiceList.end = NULL;

    for (i = 0; i < VOICES; i++)
    {
        GUSWAVE_Voices[i].num = -1;
        GUSWAVE_Voices[i].Active = FALSE;
        GUSWAVE_Voices[i].GF1voice = -1;
        GUSWAVE_Voices[i].mem = NULL;
    }

    for (i = 0; i < VOICES; i++)
    {
        GUSWAVE_Voices[i].num = i;
        GUSWAVE_Voices[i].Active = FALSE;
        GUSWAVE_Voices[i].GF1voice = 0;

        GUSWAVE_Voices[i].mem = gf1_malloc(GF1BSIZE);
        if (GUSWAVE_Voices[i].mem == 0) // Compare to NULL
        {
            GUSWAVE_MaxVoices = i;
            if (i < 1)
            {
                return (GUSWAVE_Error);
            }

            break;
        }

        LL_AddToTail(VoiceNode, &VoicePool, &GUSWAVE_Voices[i]);
    }

    return (GUSWAVE_Ok);
}

/*---------------------------------------------------------------------
   Function: GUSWAVE_Init

   Initializes the Gravis Ultrasound for digitized sound playback.
---------------------------------------------------------------------*/

int GUSWAVE_Init()
{
    int status;
    int i;

    if (GUSWAVE_Installed)
    {
        GUSWAVE_Shutdown();
    }

    // Init silence tables
    GUS_Silence8 = (unsigned char *)Z_MallocUnowned(1024, PU_STATIC);
    for (i = 0; i < 1024; i++)
    {
        GUS_Silence8[i] = 0x80;
    }

    GUS_Silence16 = (unsigned short *)Z_MallocUnowned(1024, PU_STATIC);
    for (i = 0; i < 512; i++)
    {
        GUS_Silence16[i] = 0x00;
    }

    status = GUS_Init();
    if (status != GUS_Ok)
    {
        return (GUSWAVE_Error);
    }

    GUSWAVE_MaxVoices = 2;

    status = GUSWAVE_InitVoices();
    if (status != GUSWAVE_Ok)
    {
        GUS_Shutdown();
        return (status);
    }

    GUSWAVE_SetReverseStereo(FALSE);

    GUSWAVE_Installed = TRUE;

    return (GUSWAVE_Ok);
}
