#include <stdio.h>
#include "ns_cd.h"
#include "i_system.h"

//#define CD_LOG

#ifdef CD_LOG
#include "i_debug.h"
#endif

#pragma pack(1);

struct DPMI_PTR
{
    unsigned short int segment;
    unsigned short int selector;
};

struct CD_Cdrom_data CD_Cdrom_data;
struct CD_Volumeinfo CD_Volumeinfo;

typedef struct CD_Playinfo
{
    unsigned char Control;
    unsigned char Adr;
    unsigned char Track;
    unsigned char Index;
    unsigned char Min;
    unsigned char Sec;
    unsigned char Frame;
    unsigned char Zero;
    unsigned char Amin;
    unsigned char Asec;
    unsigned char Aframe;
} CD_Playinfo;

static struct rminfo
{
    unsigned long EDI;
    unsigned long ESI;
    unsigned long EBP;
    unsigned long reserved_by_system;
    unsigned long EBX;
    unsigned long EDX;
    unsigned long ECX;
    unsigned long EAX;
    unsigned short flags;
    unsigned short ES, DS, FS, GS, IP, CS, SP, SS;
} RMI;

static struct DPMI_PTR CD_Device_req = {0, 0};
static struct DPMI_PTR CD_Device_extra = {0, 0};
static union REGS regs;
static struct SREGS sregs;

unsigned long TrackBeginPosition[MAX_TRACKS];
unsigned long TrackLength[MAX_TRACKS];

static void PrepareRegisters(void)
{
    memset(&RMI, 0, sizeof(RMI));
    memset(&sregs, 0, sizeof(sregs));
    memset(&regs, 0, sizeof(regs));
}

static void RMIRQ2F()
{
    memset(&regs, 0, sizeof(regs));
    regs.w.ax = 0x0300;
    regs.h.bl = 0x02F;
    sregs.es = FP_SEG(&RMI);
    regs.x.edi = FP_OFF(&RMI);
    int386x(0x31, &regs, &regs, &sregs);
}

void DPMI_AllocDOSMem(short int paras, struct DPMI_PTR *p)
{
    PrepareRegisters();
    regs.w.ax = 0x0100;
    regs.w.bx = paras;
    int386x(0x31, &regs, &regs, &sregs);
    p->segment = regs.w.ax;
    p->selector = regs.w.dx;
}

void DPMI_FreeDOSMem(struct DPMI_PTR *p)
{
    memset(&sregs, 0, sizeof(sregs));
    regs.w.ax = 0x0101;
    regs.w.dx = p->selector;
    int386x(0x31, &regs, &regs, &sregs);
}

void CD_DeviceRequest(void)
{
    #ifdef CD_LOG
    I_Printf("CD_DeviceRequest: Execute MSCDEX request\n");
    #endif

    PrepareRegisters();
    RMI.EAX = 0x01510;
    RMI.ECX = CD_Cdrom_data.First_drive;
    RMI.EDI = 0;
    RMI.ES = CD_Device_req.segment;
    RMIRQ2F();
}

void Red_book(unsigned long Value, unsigned char *min, unsigned char *sec, unsigned char *frame)
{
    *frame = (Value & 0x000000FF);
    *sec = (Value & 0x0000FF00) >> 8;
    *min = (Value & 0x00FF0000) >> 16;
}

unsigned long HSG(unsigned long Value)
{
    unsigned char min, sec, frame;

    Red_book(Value, &min, &sec, &frame);
    Value = (unsigned long)min * 4500;
    Value += (short)sec * 75;
    Value += frame - 150;
    return (Value);
}

short CD_CdromInstalled(void)
{
    #ifdef CD_LOG
    I_Printf("CD_CdromInstalled: Alloc DOS MEM\n");
    #endif

    DPMI_AllocDOSMem(4, &CD_Device_req);
    DPMI_AllocDOSMem(2, &CD_Device_extra);

    #ifdef CD_LOG
    I_Printf("CD_CdromInstalled: Prepare registers\n");
    #endif

    PrepareRegisters();
    regs.x.eax = 0x01500;

    #ifdef CD_LOG
    I_Printf("CD_CdromInstalled: Call 0x2F IRQ\n");
    #endif

    int386(0x02F, &regs, &regs);

    if (regs.x.ebx == 0)
        return (0);

    #ifdef CD_LOG
    I_Printf("CD_CdromInstalled: MSCDEX OK, Drive %u\n", regs.x.ebx);
    #endif

    CD_Cdrom_data.Drives = (short)regs.x.ebx;
    CD_Cdrom_data.First_drive = (short)regs.x.ecx;
    CD_Status();
    CD_Mediach();
    CD_StopAudio();
    CD_GetAudioInfo();
    return (1);
}

void CD_GetAudioInfo(void)
{
    typedef struct IOCTLI
    {
        unsigned char Length;
        unsigned char Subunit;
        unsigned char Comcode;
        unsigned short Status;
        unsigned char Unused[8];
        unsigned char Media;
        unsigned long Address;
        unsigned short Bytes;
        unsigned short Sector;
        unsigned long VolID;
    } IOCTLI;
    typedef struct Track_data
    {
        unsigned char Mode;
        unsigned char Lowest;
        unsigned char Highest;
        unsigned long Address;
    } Track_data;

    static struct IOCTLI *IOCTLI_Pointers;
    static struct Track_data *Track_data_Pointers;

    #ifdef CD_LOG
    I_Printf("CD_GetAudioInfo: Get AudioCD information\n");
    #endif

    IOCTLI_Pointers = (struct IOCTLI *)(CD_Device_req.segment * 16);
    Track_data_Pointers = (struct Track_data *)(CD_Device_extra.segment * 16);

    memset(IOCTLI_Pointers, 0, sizeof(struct IOCTLI));
    memset(Track_data_Pointers, 0, sizeof(struct Track_data));

    IOCTLI_Pointers->Length = sizeof(struct IOCTLI);
    IOCTLI_Pointers->Comcode = 3;
    IOCTLI_Pointers->Address = CD_Device_extra.segment << 16;
    IOCTLI_Pointers->Bytes = sizeof(struct Track_data);
    Track_data_Pointers->Mode = 0x0A;
    CD_DeviceRequest();

    memcpy(&CD_Cdrom_data.DiskID, &Track_data_Pointers->Lowest, 6);
    CD_Cdrom_data.Low_audio = Track_data_Pointers->Lowest;
    CD_Cdrom_data.High_audio = Track_data_Pointers->Highest;
    Red_book(Track_data_Pointers->Address, &CD_Cdrom_data.Disk_length_min, &CD_Cdrom_data.Disk_length_sec, &CD_Cdrom_data.Disk_length_frames);
    CD_Cdrom_data.Endofdisk = HSG(Track_data_Pointers->Address);
    CD_Cdrom_data.Error = IOCTLI_Pointers->Status;
}

void CD_GetAudioStatus(void)
{
    typedef struct IOCTLI
    {
        unsigned char Length;
        unsigned char Subunit;
        unsigned char Comcode;
        unsigned short Status;
        unsigned char Unused[8];
        unsigned char Media;
        unsigned long Address;
        unsigned short Bytes;
        unsigned short Sector;
        unsigned long VolID;
    } IOCTLI;
    typedef struct Track_data
    {
        unsigned char Mode;
        unsigned char Lowest;
        unsigned char Highest;
        unsigned long Address;
    } Track_data;

    static struct IOCTLI *IOCTLI_Pointers;
    static struct Track_data *Track_data_Pointers;

    #ifdef CD_LOG
    I_Printf("CD_GetAudioStatus\n");
    #endif

    IOCTLI_Pointers = (struct IOCTLI *)(CD_Device_req.segment * 16);
    Track_data_Pointers = (struct Track_data *)(CD_Device_extra.segment * 16);

    memset(IOCTLI_Pointers, 0, sizeof(struct IOCTLI));
    memset(Track_data_Pointers, 0, sizeof(struct Track_data));

    IOCTLI_Pointers->Length = sizeof(struct IOCTLI);
    IOCTLI_Pointers->Comcode = 3;
    IOCTLI_Pointers->Address = CD_Device_extra.segment << 16;
    IOCTLI_Pointers->Bytes = sizeof(struct Track_data);
    Track_data_Pointers->Mode = 0x0A;
    CD_DeviceRequest();

    CD_Cdrom_data.Status = IOCTLI_Pointers->Status;

    #ifdef CD_LOG
    I_Printf("CD_GetAudioStatus: %u\n", CD_Cdrom_data.Status);
    #endif
}

unsigned long CD_GetTrackLength(short Tracknum)
{
    unsigned long Start, Finish;
    unsigned short CT;

    CT = CD_Cdrom_data.Current_track;
    CD_SetTrack(Tracknum);
    Start = CD_Cdrom_data.Track_position;
    if (Tracknum < CD_Cdrom_data.High_audio)
    {
        CD_SetTrack(Tracknum + 1);
        Finish = CD_Cdrom_data.Track_position;
    }
    else
        Finish = CD_Cdrom_data.Endofdisk;

    CD_SetTrack(CT);

    Finish -= Start;
    return (Finish);
}

void CD_SetTrack(short Tracknum)
{
    typedef struct Tray_request
    {
        unsigned char Length;
        unsigned char Subunit;
        unsigned char Comcode;
        unsigned short Status;
        unsigned char Unused[8];
        unsigned char Media;
        unsigned long Address;
        unsigned short Bytes;
        unsigned short Sector;
        unsigned long VolID;
    } Tray_request;
    typedef struct Track_data
    {
        unsigned char Mode;
        unsigned char Track;
        unsigned long Address;
        unsigned char Control;
    } Head_data;

    static struct Tray_request *Tray_request_Pointers;
    static struct Track_data *Track_data_Pointers;

    Tray_request_Pointers = (struct Tray_request *)(CD_Device_req.segment * 16);
    Track_data_Pointers = (struct Track_data *)(CD_Device_extra.segment * 16);

    memset(Tray_request_Pointers, 0, sizeof(struct Tray_request));
    memset(Track_data_Pointers, 0, sizeof(struct Track_data));

    Tray_request_Pointers->Length = sizeof(struct Tray_request);
    Tray_request_Pointers->Comcode = 3;
    Tray_request_Pointers->Address = CD_Device_extra.segment << 16;
    Tray_request_Pointers->Bytes = 7;

    Track_data_Pointers->Mode = 0x0B;
    Track_data_Pointers->Track = Tracknum;

    CD_DeviceRequest();

    CD_Cdrom_data.Error = Tray_request_Pointers->Status;
    CD_Cdrom_data.Track_position = HSG(Track_data_Pointers->Address);
    CD_Cdrom_data.Current_track = Tracknum;
    CD_Cdrom_data.Track_type = Track_data_Pointers->Control & TRACK_MASK;
}

void CD_Status(void)
{
    typedef struct Tray_request
    {
        unsigned char Length;
        unsigned char Subunit;
        unsigned char Comcode;
        unsigned short Status;
        unsigned char Unused[8];
        unsigned char Media;
        unsigned long Address;
        unsigned short Bytes;
        unsigned short Sector;
        unsigned long VolID;
    } Tray_request;
    typedef struct CD_data
    {
        unsigned char Mode;
        unsigned long Status;
    } CD_data;

    static struct Tray_request *Tray_request_Pointers;
    static struct CD_data *CD_data_Pointers;

    #ifdef CD_LOG
    I_Printf("CD_Status: Get MSCDEX status\n");
    #endif

    Tray_request_Pointers = (struct Tray_request *)(CD_Device_req.segment * 16);
    CD_data_Pointers = (struct CD_data *)(CD_Device_extra.segment * 16);

    memset(Tray_request_Pointers, 0, sizeof(struct Tray_request));
    memset(CD_data_Pointers, 0, sizeof(struct CD_data));

    Tray_request_Pointers->Length = sizeof(struct Tray_request);
    Tray_request_Pointers->Comcode = 3;
    Tray_request_Pointers->Address = CD_Device_extra.segment << 16;
    Tray_request_Pointers->Bytes = 5;
    CD_data_Pointers->Mode = 0x06;

    CD_DeviceRequest();

    CD_Cdrom_data.Error = Tray_request_Pointers->Status;
    CD_Cdrom_data.Status = CD_data_Pointers->Status;
}

void CD_StopAudio(void)
{
    typedef struct Stop_request
    {
        unsigned char Length;
        unsigned char Subunit;
        unsigned char Comcode;
        unsigned short Status;
        unsigned char Unused[8];
    } Stop_request;

    static struct Stop_request *Stop_request_Pointers;

    #ifdef CD_LOG
    I_Printf("CD_StopAudio\n");
    #endif

    Stop_request_Pointers = (struct Stop_request *)(CD_Device_req.segment * 16);

    memset(Stop_request_Pointers, 0, sizeof(struct Stop_request));

    Stop_request_Pointers->Length = sizeof(struct Stop_request);
    Stop_request_Pointers->Comcode = 133;

    CD_DeviceRequest();

    CD_Cdrom_data.Error = Stop_request_Pointers->Status;
}

void CD_ResumeAudio(void)
{
    typedef struct Stop_request
    {
        unsigned char Length;
        unsigned char Subunit;
        unsigned char Comcode;
        unsigned short Status;
        unsigned char Unused[8];
    } Stop_request;

    static struct Stop_request *Stop_request_Pointers;

    #ifdef CD_LOG
    I_Printf("CD_ResumeAudio\n");
    #endif

    Stop_request_Pointers = (struct Stop_request *)(CD_Device_req.segment * 16);

    memset(Stop_request_Pointers, 0, sizeof(struct Stop_request));

    Stop_request_Pointers->Length = sizeof(struct Stop_request);
    Stop_request_Pointers->Comcode = 136;

    CD_DeviceRequest();

    CD_Cdrom_data.Error = Stop_request_Pointers->Status;
}

void CD_PlayAudio(unsigned long Begin, unsigned long Length)
{
    typedef struct Play_request
    {
        unsigned char Length;
        unsigned char Subunit;
        unsigned char Comcode;
        unsigned short Status;
        unsigned char Unused[8];
        unsigned char Addressmode;
        unsigned long Start;
        unsigned long Playlength;
    } Play_request;

    static struct Play_request *Play_request_Pointers;

    #ifdef CD_LOG
    I_Printf("CD_PlayAudio (%u, %u)\n", Begin, Length);
    #endif

    Play_request_Pointers = (struct Play_request *)(CD_Device_req.segment * 16);

    memset(Play_request_Pointers, 0, sizeof(struct Play_request));

    Play_request_Pointers->Length = sizeof(struct Play_request);
    Play_request_Pointers->Comcode = 132;
    Play_request_Pointers->Start = Begin;
    Play_request_Pointers->Playlength = Length;

    CD_DeviceRequest();

    CD_Cdrom_data.Error = Play_request_Pointers->Status;
}

void CD_CMD(unsigned char Mode)
{
    typedef struct Tray_request
    {
        unsigned char Length;
        unsigned char Subunit;
        unsigned char Comcode;
        unsigned short Status;
        unsigned char Unused[8];
        unsigned char Media;
        unsigned long Address;
        unsigned short Bytes;
        unsigned short Sector;
        unsigned long VolID;
        unsigned char Unused2[4];
    } Tray_request;
    typedef struct CD_Mode
    {
        unsigned char Mode;
    } CD_Mode;

    static struct Tray_request *Tray_request_Pointers;
    static struct CD_Mode *CD_Mode_Pointers;

    Tray_request_Pointers = (struct Tray_request *)(CD_Device_req.segment * 16);
    CD_Mode_Pointers = (struct CD_Mode *)(CD_Device_extra.segment * 16);

    memset(Tray_request_Pointers, 0, sizeof(struct Tray_request));
    memset(CD_Mode_Pointers, 0, sizeof(struct CD_Mode));

    CD_Mode_Pointers->Mode = Mode;
    Tray_request_Pointers->Length = sizeof(struct Tray_request);
    Tray_request_Pointers->Comcode = 12;
    Tray_request_Pointers->Address = CD_Device_extra.segment << 16;
    Tray_request_Pointers->Bytes = 1;

    CD_DeviceRequest();

    CD_Cdrom_data.Error = Tray_request_Pointers->Status;
}

void CD_Lock(unsigned char Doormode)
{
    typedef struct Tray_request
    {
        unsigned char Length;
        unsigned char Subunit;
        unsigned char Comcode;
        unsigned short Status;
        unsigned char Unused[8];
        unsigned char Media;
        unsigned long Address;
        unsigned short Bytes;
        unsigned char Unused2[4];
    } Tray_request;

    typedef struct CD_Data
    {
        unsigned char Mode;
        unsigned char Media;
    } CD_Data;

    static struct Tray_request *Tray_request_Pointers;
    static struct CD_Data *CD_Data_Pointers;

    Tray_request_Pointers = (struct Tray_request *)(CD_Device_req.segment * 16);
    CD_Data_Pointers = (struct CD_Data *)(CD_Device_extra.segment * 16);

    memset(Tray_request_Pointers, 0, sizeof(struct Tray_request));
    memset(CD_Data_Pointers, 0, sizeof(struct CD_Data));

    Tray_request_Pointers->Length = sizeof(struct Tray_request);
    Tray_request_Pointers->Comcode = 12;
    Tray_request_Pointers->Address = CD_Device_extra.segment << 16;
    Tray_request_Pointers->Bytes = 2;
    CD_Data_Pointers->Mode = 1;
    CD_Data_Pointers->Media = Doormode;

    CD_DeviceRequest();

    CD_Cdrom_data.Error = Tray_request_Pointers->Status;
}

short CD_Mediach(void)
{
    typedef struct Tray_request
    {
        unsigned char Length;
        unsigned char Subunit;
        unsigned char Comcode;
        unsigned short Status;
        unsigned char Unused[8];
        unsigned char Media;
        unsigned long Address;
        unsigned short Bytes;
        unsigned short Sector;
        unsigned long VolID;
    } Tray_request;

    typedef struct CD_Data
    {
        unsigned char Mode;
        unsigned char Media;
    } CD_Data;

    static struct Tray_request *Tray_request_Pointers;
    static struct CD_Data *CD_Data_Pointers;

    #ifdef CD_LOG
    I_Printf("CD_Mediach: Check disk\n");
    #endif

    Tray_request_Pointers = (struct Tray_request *)(CD_Device_req.segment * 16);
    CD_Data_Pointers = (struct CD_Data *)(CD_Device_extra.segment * 16);

    memset(Tray_request_Pointers, 0, sizeof(struct Tray_request));
    memset(CD_Data_Pointers, 0, sizeof(struct CD_Data));

    Tray_request_Pointers->Length = sizeof(struct Tray_request);
    Tray_request_Pointers->Comcode = 3;
    Tray_request_Pointers->Address = CD_Device_extra.segment << 16;
    Tray_request_Pointers->Bytes = 2;
    CD_Data_Pointers->Mode = 0x09;

    CD_DeviceRequest();

    CD_Cdrom_data.Error = Tray_request_Pointers->Status;
    return (CD_Data_Pointers->Media);
}

void CD_SetVolume(unsigned char vol)
{
    typedef struct Tray_request
    {
        unsigned char Length;
        unsigned char Subunit;
        unsigned char Comcode;
        unsigned short Status;
        unsigned char Unused[8];
        unsigned char Media;
        unsigned long Address;
        unsigned short Bytes;
        unsigned short Sector;
        unsigned long VolID;
    } Tray_request;

    static struct Tray_request *Tray_request_Pointers;
    static struct CD_Volumeinfo *CD_Volume_Pointers;

    #ifdef CD_LOG
    I_Printf("CD_SetVolume %u\n", vol);
    #endif

    CD_Volumeinfo.Volume0 = vol;
    CD_Volumeinfo.Volume1 = vol;
    CD_Volumeinfo.Volume2 = vol;
    CD_Volumeinfo.Volume3 = vol;

    Tray_request_Pointers = (struct Tray_request *)(CD_Device_req.segment * 16);
    CD_Volume_Pointers = (struct CD_Volumeinfo *)(CD_Device_extra.segment * 16);

    memset(Tray_request_Pointers, 0, sizeof(struct Tray_request));
    memcpy(CD_Volume_Pointers, &CD_Volumeinfo, sizeof(struct CD_Volumeinfo));
    Tray_request_Pointers->Length = sizeof(struct Tray_request);
    Tray_request_Pointers->Comcode = 12;
    Tray_request_Pointers->Address = CD_Device_extra.segment << 16;
    Tray_request_Pointers->Bytes = 9;
    CD_Volume_Pointers->Mode = 0x03;

    CD_DeviceRequest();

    CD_Cdrom_data.Error = Tray_request_Pointers->Status;
}

void CD_Exit(void)
{
    CD_StopAudio();
    CD_Lock(UNLOCK);
    CD_DeInit();
}

void CD_DeInit(void)
{
    DPMI_FreeDOSMem(&CD_Device_req);
    DPMI_FreeDOSMem(&CD_Device_req);
}

int CD_Init(void)
{
    #ifdef CD_LOG
    I_Printf("CD_Init\n");
    #endif

    if (!CD_CdromInstalled())
    {
        #ifdef CD_LOG
        I_Printf("CD_CdromInstalled failed\n");
        #endif

        I_Error("MSCDEX was not found\n");
        return 0;
    }
    else
    {
        int i;

        #ifdef CD_LOG
        I_Printf("CD_CdromInstalled OK\n");
        I_Printf("CD_Init: %d AudioCD tracks\n", CD_Cdrom_data.High_audio);
        #endif

        printf("MSCDEX found\n");
        printf("Tracks: %d\n", CD_Cdrom_data.High_audio);

        if (!CD_Cdrom_data.High_audio)
        {
            // No tracks!

            #ifdef CD_LOG
            I_Printf("CD_Init: NO AudioCD tracks available\n");
            #endif

            I_Error("No Audio-CD tracks available\n");
            return 0;
        }

        for (i = 0; i < MAX_TRACKS; i++)
        {
            TrackBeginPosition[i] = 0;
            TrackLength[i] = 0;
        }

        #ifdef CD_LOG
        I_Printf("CD_Init: Cache AudioCD information\n");
        #endif

        // Cache track begin and end position
        for (i = 1; i <= CD_Cdrom_data.High_audio; i++)
        {
            CD_SetTrack(i);
            TrackBeginPosition[i] = CD_Cdrom_data.Track_position;
            TrackLength[i] = CD_GetTrackLength(i);

            #ifdef CD_LOG
            I_Printf("CD_Init: Track %d, Begin %u, Length %u\n", i, TrackBeginPosition[i], TrackLength[i]);
            #endif
        }

        return 1;
    }
}
