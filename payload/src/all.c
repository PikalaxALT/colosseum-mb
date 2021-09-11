#include "global.h"
#include "gflib.h"
#include "libpmagb.h"
#include "berry_fix.h"
#include "constants/pokemon.h"

struct UnkStruct_02024960
{
    u32 unk_00:24;
    u32 unk_03_0:7;
    u32 unk_03_7:1;
    u8 filler_04[0x854];
    vu8 unk_858;
    u8 filler_859[0x22];
    u8 unk_87B;
    u8 filler_87C[2];
    u8 unk_87E;
    u8 unk_87F;
    u32 unk_880;
};

EWRAM_DATA volatile struct UnkStruct_02024960 gUnknown_02024960 = {0};

u32 sub_02006490(void);
void sub_02002A9C(u8, u32, u8);

static u8 gUnknown_0201F3AC[256] = {0};
static IntrFunc * gUnknown_0201F4AC = &gIntrTable[6];

u8 gSaveReadResult;
u8 gBerryFixResult;

void sub_020098D8(u8, IntrFunc*);
extern void main_callback(u32 *pstate, u8 *buffer, u32 unk2);
extern struct UnkStruct_02023F50 * BuildPlayerSaveProfile(struct SaveBlock2 *, struct SaveBlock1 *, int);
extern void sub_0200D924(const char *);
extern int VBlankCB_default(void);
void FadeOut(void);
bool32 sub_020047D4(void);
void sub_02006344(void);
void sub_02005BB8(void);
bool32 sub_020064B0(void);
void sub_02005704(int);

bool8 sub_0200023C(void)
{
    u32 r0;

    sub_02006490();
    if (gUnknown_02024960.unk_03_0 == 0)
    {
        gUnknown_02024960.unk_858 = 1;
    }
    else
    {
        gUnknown_02024960.unk_858 = 2;
    }
    sub_02002A9C(0, sub_02006490(), 1);
    r0 = gUnknown_02024960.unk_87E | (gUnknown_02024960.unk_87B << 16);
    gUnknown_02024960.unk_880 = r0;
    gUnknown_02024960.unk_87F = 1;
    return FALSE;
}

static inline int QuietlyFixBerryGlitch(void)
{
    u32 state = MAINCB_INIT;
    u8 buffer[0x40];
    int ret = 0;
    // r6 should be set with 0x020000AC in both branches
    if (gAgbPmRomParams->gameLanguage == LANGUAGE_ENGLISH)
        return 0;
    while (1)
    // r5 should not be set
    {
        main_callback(&state, buffer, 0);
        if (state == MAINCB_FIX_DATE)
        {
            ret = 1;
        }
        if (state == MAINCB_ERROR)
        {
            ret = 2;
        }
        if (state == MAINCB_NO_NEED_TO_FIX || state == MAINCB_YEAR_MAKES_NO_SENSE || state == MAINCB_ERROR)
            break;
    }
    return ret;
}

void InitStruct2021B60(void);

void GF_Main(void)
{
    int i;
    u16 status;

    DetectROM();
    sub_020098D8(2, gUnknown_0201F4AC);
    SaveBlocksInit();
    SetSaveSectorPtrs();
    gSaveReadResult = ReadSaveBlockChunks();
    gBerryFixResult = QuietlyFixBerryGlitch(); // r5 should be &gBerryFixResult
    InitSound();
    InitStruct2021B60();
    SetKeyRepeatTiming(40, 5);
    REG_IE = INTR_FLAG_VBLANK;
    REG_DISPSTAT = DISPSTAT_VBLANK_INTR;
    REG_DISPCNT &= ~DISPCNT_FORCED_BLANK;
    REG_IME = 1;
    BuildPlayerSaveProfile(gSaveBlock2Ptr, gSaveBlock1Ptr, gSaveReadResult);
    sub_0200D924((char *)0x020000AC); // should be loaded into r6 inside QuietlyFixBerryGlitch
    SetVBlankCallback(VBlankCB_default);
    PauseSoundVSync();
    GenerateFontHalfrowLookupTable((void *)(IWRAM_START + 0x4000));
    FadeOut();
    for (;;)
    {
        if (!sub_020047D4())
        {
            sub_02006344();
            sub_02005BB8();
            if (!sub_020064B0())
            {
                if (gUnknown_02024960.unk_03_0 != 0)
                    sub_02005704(gUnknown_02024960.unk_03_0 - 1);
                else
                    sub_02005704(0);
            }
            else
                sub_0200023C();
        }
        else
        {
            CpuCopy16(gSaveBlock1BakPtr, gSaveBlock1Ptr, gAgbPmRomParams->saveBlock1Size);
            status = 0;
            for (i = 0; i < PARTY_SIZE; i++)
            {
                SetMonData(&gPlayerPartyPtr[i], MON_DATA_STATUS, (u8 *)&status);
            }
            sub_02002A9C(1, 0, 0);
        }
    }
}
