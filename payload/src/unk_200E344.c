#include "global.h"
#include "siirtc.h"
#include "constants/vars.h"
#include "berry_fix.h"

u32 gUpdateSuccessful;
u32 sGameVersion;
struct Time gTimeSinceBerryUpdate;
struct Time gRtcUTCTime;
u32 padding_2025270;

static u32 gInitialWaitTimer;
static struct SiiRtcInfo sRtcInfoWork;
static u32 padding_2021394;
static u16 sRtcProbeStatus;
static u32 padding_202139C;
static struct SiiRtcInfo sRtcInfoBuffer;
static u8 sRtcProbeCode;
static u16 sImeBak;

bool32 WriteSaveBlockChunks(void);

bool32 rtc_maincb_is_rtc_working(void);
bool32 rtc_maincb_is_time_since_last_berry_update_positive(u8 *);
void rtc_maincb_fix_date(void);
bool32 check_pacifidlog_tm_received_day_before_today(void);
bool32 bfix_fix_pacifidlog_tm(void);
void rtc_probe_status(void);
u16 rtc_get_probe_status(void);
void rtc_get_status_and_datetime(struct SiiRtcInfo * rtc);
s32 bcd_to_hex(u8 year);
void rtc_sub_time_from_datetime(struct SiiRtcInfo * rtc, struct Time * work, struct Time * offset);
void rtc_sub_time_from_time(struct Time * work, struct Time * last, struct Time * result);
void rtc_intr_disable(void);
void rtc_intr_enable(void);
bool8 is_leap_year(u8 year);
u16 rtc_validate_datetime(struct SiiRtcInfo * info);

const u8 sLanguageCodeAndRevisionLUT[6][2] = {
    {'J', 1},
    {'E', 2},
    {'D', 1},
    {'F', 1},
    {'I', 1},
    {'S', 1}
};

const char sTitleCode_Ruby[16] = "POKEMON RUBYAXV";
const char sTitleCode_Sapphire[16] = "POKEMON SAPPAXP";

bool32 memcmp_u8(const char * a0, const char * a1, u32 a2)
{
    u32 i;

    for (i = 0; i < a2; i++)
    {
        if (a0[i] != a1[i])
        {
            return FALSE;
        }
    }

    return TRUE;
}

s32 validate_rom_header_internal(void)
{
    s32 i;
    s32 foundRomParam;
    s32 languageCode;
    s32 version;

    languageCode = *(const u8 *)0x080000AF;
    version = *(const u8 *)0x080000BC;
    foundRomParam = -1;
    for (i = 0; i < NELEMS(sLanguageCodeAndRevisionLUT); i++)
    {
        if (languageCode != sLanguageCodeAndRevisionLUT[i][0])
        {
            continue;
        }
        if (version >= sLanguageCodeAndRevisionLUT[i][1])
        {
            foundRomParam = 0;
        }
        else
        {
            foundRomParam = 1;
        }
        break;
    }
    if (foundRomParam == -1)
    {
        return INVALID;
    }
    if (memcmp_u8((const u8 *)0x080000A0, sTitleCode_Ruby, 15) == TRUE)
    {
        if (foundRomParam == 0)
        {
            return RUBY_NONEED;
        }
        else
        {
            sGameVersion = VERSION_RUBY;
            return RUBY_UPDATABLE;
        }
    }
    if (memcmp_u8((const u8 *)0x080000A0, sTitleCode_Sapphire, 15) == TRUE)
    {
        if (foundRomParam == 0)
        {
            return SAPPHIRE_NONEED;
        }
        else
        {
            sGameVersion = VERSION_SAPPHIRE;
            return SAPPHIRE_UPDATABLE;
        }
    }

    return INVALID;
}

s32 validate_rom_header(void)
{
    if (*(u8 *)0x080000B0 != '0')
        return INVALID;
    
    if (*(u8 *)0x080000B1 != '1')
        return INVALID;
    
    if (*(u8 *)0x080000B2 != 0x96)
        return INVALID;
    
    return validate_rom_header_internal();
}

void main_callback(u32 * pstate, u8 * buffer, u32 unk2)
{
    u8 yearLo;
    int romHeaderValidParam;
    switch (*pstate)
    {
    case MAINCB_INIT:
        gInitialWaitTimer = 0;
        gUpdateSuccessful = 0;
        romHeaderValidParam = validate_rom_header();
        switch (romHeaderValidParam)
        {
        case INVALID:
            *pstate = MAINCB_ERROR;
            break;
        case RUBY_NONEED:
        case SAPPHIRE_NONEED:
            *pstate = MAINCB_NO_NEED_TO_FIX;
            break;
        case RUBY_UPDATABLE:
        case SAPPHIRE_UPDATABLE:
            (*pstate)++;
            break;
        }
        break;
    case MAINCB_CHECK_RTC:
        if (rtc_maincb_is_rtc_working() == FALSE)
        {
            *pstate = MAINCB_ERROR;
        }
        else
        {
            (*pstate)++;
        }
        break;
    case MAINCB_CHECK_FLASH:
        (*pstate)++;
        break;
    case MAINCB_READ_SAVE:
        (*pstate)++;
        break;
    case MAINCB_CHECK_TIME:
        if (rtc_maincb_is_time_since_last_berry_update_positive(&yearLo) == TRUE)
        {
            if (yearLo == 0) // 2000
            {
                (*pstate)++;
            }
            else
            {
                *pstate = MAINCB_CHECK_PACIFIDLOG_TM;
            }
        }
        else
        {
            if (yearLo != 1) // 2001
            {
                *pstate = MAINCB_YEAR_MAKES_NO_SENSE;
            }
            else
            {
                (*pstate)++;
            }
        }
        break;
    case MAINCB_FIX_DATE:
        rtc_maincb_fix_date();
        gUpdateSuccessful |= 1;
        *pstate = MAINCB_CHECK_PACIFIDLOG_TM;
        break;
    case MAINCB_CHECK_PACIFIDLOG_TM:
        if (check_pacifidlog_tm_received_day_before_today() == TRUE)
        {
            *pstate = MAINCB_FINISHED;
        }
        else
        {
            *pstate = MAINCB_FIX_PACIFIDLOG_TM;
        }
        break;
    case MAINCB_FIX_PACIFIDLOG_TM:
        if (bfix_fix_pacifidlog_tm() == TRUE)
        {
            gUpdateSuccessful |= 1;
            *pstate = MAINCB_FINISHED;
        }
        else
        {
            *pstate = MAINCB_ERROR;
        }
        break;
    case MAINCB_FINISHED:
        *pstate = MAINCB_NO_NEED_TO_FIX;
        break;
    case MAINCB_NO_NEED_TO_FIX:
        break;
    case MAINCB_YEAR_MAKES_NO_SENSE:
        break;
    case MAINCB_ERROR:
        break;
    }
}

bool32 rtc_maincb_is_rtc_working(void)
{
    rtc_probe_status();
    if (rtc_get_probe_status() & 0xFF0)
        return FALSE;
    else
        return TRUE;
}

void rtc_set_datetime(struct SiiRtcInfo *rtc)
{
    vu16 imeBak;
    imeBak = REG_IME;
    REG_IME = 0;
    SiiRtcSetDateTime(rtc);
    REG_IME = imeBak;
}

bool32 rtc_maincb_is_time_since_last_berry_update_positive(u8 * year_p)
{
    s32 totalMinutes;
    rtc_get_status_and_datetime(&sRtcInfoWork);
    *year_p = bcd_to_hex(sRtcInfoWork.year);
    rtc_sub_time_from_datetime(&sRtcInfoWork, &gRtcUTCTime, &((struct SaveBlock2 *)gSaveBlock2Ptr)->localTimeOffset);
    rtc_sub_time_from_time(&gTimeSinceBerryUpdate, &((struct SaveBlock2 *)gSaveBlock2Ptr)->lastBerryTreeUpdate, &gRtcUTCTime);
    totalMinutes = gTimeSinceBerryUpdate.days * 1440 + gTimeSinceBerryUpdate.hours * 60 + gTimeSinceBerryUpdate.minutes;
    if (totalMinutes >= 0)
        return TRUE;
    else
        return FALSE;
}

s32 hex_to_bcd(u8 a0)
{
    s32 ret = 0;
    if (a0 > 99)
    {
        return 0xFF;
    }
    ret  = Div(a0, 10) << 4;
    ret |= Mod(a0, 10);
    return ret;
}

void sii_rtc_inc(u8 * a0)
{
    *a0 = hex_to_bcd(bcd_to_hex(*a0) + 1);
}

void sii_rtc_inc_month(struct SiiRtcInfo * a0)
{
    sii_rtc_inc(&a0->month);
    if (bcd_to_hex(a0->month) <= MONTH_DEC)
    {
        return;
    }
    sii_rtc_inc(&a0->year);
    a0->month = MONTH_JAN;
}

const s32 sDaysPerMonth[];

void sii_rtc_inc_day(struct SiiRtcInfo * a0)
{
    sii_rtc_inc(&a0->day);
    if (bcd_to_hex(a0->day) <= sDaysPerMonth[bcd_to_hex(a0->month) - 1])
    {
        return;
    }
    if (is_leap_year(bcd_to_hex(a0->year)) && bcd_to_hex(a0->month) == MONTH_FEB && bcd_to_hex(a0->day) == 29)
    {
        return;
    }
    a0->day = 1;
    sii_rtc_inc_month(a0);
}

bool32 rtc_is_past_feb_28_2000(struct SiiRtcInfo * a0)
{
    if (bcd_to_hex(a0->year) == 0)
    {
        if (bcd_to_hex(a0->month) == MONTH_JAN)
            return FALSE;
        if (bcd_to_hex(a0->month) > MONTH_FEB)
            return TRUE;
        if (bcd_to_hex(a0->day) == 29)
            return TRUE;
        return FALSE;
    }
    if (bcd_to_hex(a0->year) == 1)
        return TRUE;
    return FALSE;
}

void rtc_maincb_fix_date(void)
{
    rtc_get_status_and_datetime(&sRtcInfoWork);
    if (bcd_to_hex(sRtcInfoWork.year) == 0 || bcd_to_hex(sRtcInfoWork.year) == 1)
    {
        if (bcd_to_hex(sRtcInfoWork.year) == 1)
        {
            sRtcInfoWork.year = 2;
            sRtcInfoWork.month = MONTH_JAN;
            sRtcInfoWork.day = 2;
            rtc_set_datetime(&sRtcInfoWork);
        }
        else
        {
            if (rtc_is_past_feb_28_2000(&sRtcInfoWork) == TRUE)
            {
                sii_rtc_inc_day(&sRtcInfoWork);
                sii_rtc_inc(&sRtcInfoWork.year);
            }
            else
            {
                sii_rtc_inc(&sRtcInfoWork.year);
            }
            rtc_set_datetime(&sRtcInfoWork);
        }
    }
}

char * print_bcd(char * a0, u8 a1)
{
    *a0 = '0' + ((a1 & 0xF0) >> 4);
    a0++;
    *a0 = '0' + ((a1 & 0x0F) >> 0);
    a0++;
    *a0 = 0;
    return a0;
}

char * print_rtc(char * a0)
{
    if (sGameVersion != 1 && sGameVersion != 2)
    {
        
    }
    else
    {
        rtc_get_status_and_datetime(&sRtcInfoWork);
    }
    a0 = print_bcd(a0, sRtcInfoWork.year);
    *a0++ = ' ';
    a0 = print_bcd(a0, sRtcInfoWork.month);
    *a0++ = ' ';
    a0 = print_bcd(a0, sRtcInfoWork.day);
    *a0++ = ' ';
    a0 = print_bcd(a0, sRtcInfoWork.hour);
    *a0++ = ':';
    a0 = print_bcd(a0, sRtcInfoWork.minute);
    *a0++ = ':';
    a0 = print_bcd(a0, sRtcInfoWork.second);
    *a0++ = 0;
    return a0;
}

const struct SiiRtcInfo sDefaultRTC = {
    .year = 0, // 2000
    .month = 1, // January
    .day = 1, // 01
    .dayOfWeek = 0,
    .hour = 0,
    .minute = 0,
    .second = 0,
    .status = 0,
    .alarmHour = 0,
    .alarmMinute = 0
};

void rtc_intr_disable(void)
{
    sImeBak = REG_IME;
    REG_IME = 0;
}

void rtc_intr_enable(void)
{
    REG_IME = sImeBak;
}

s32 bcd_to_hex(u8 a0)
{
    if (a0 >= 0xa0 || (a0 & 0xF) >= 10)
        return 0xFF;
    return ((a0 >> 4) & 0xF) * 10 + (a0 & 0xF);
}

bool8 is_leap_year(u8 year)
{
    if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
        return TRUE;
    else
        return FALSE;
}

const s32 sDaysPerMonth[] = {
    31,
    28,
    31,
    30,
    31,
    30,
    31,
    31,
    30,
    31,
    30,
    31
};

u16 rtc_count_days_parameterized(u8 year, u8 month, u8 day)
{
    u16 numDays = 0;
    s32 i;
    for (i = year - 1; i > 0; i--)
    {
        numDays += 365;
        if (is_leap_year(i) == TRUE)
            numDays++;
    }
    for (i = 0; i < month - 1; i++)
        numDays += sDaysPerMonth[i];
    if (month > MONTH_FEB && is_leap_year(year) == TRUE)
    {
        numDays = numDays + 1;
    }
    numDays += day;
    return numDays;
}

u16 rtc_count_days_from_info(struct SiiRtcInfo *info)
{
    return rtc_count_days_parameterized(bcd_to_hex(info->year), bcd_to_hex(info->month), bcd_to_hex(info->day));
}

void rtc_probe_status(void)
{
    sRtcProbeStatus = 0;
    rtc_intr_disable();
    SiiRtcUnprotect();
    sRtcProbeCode = SiiRtcProbe();
    rtc_intr_enable();
    if ((sRtcProbeCode & 0xF) != 1)
        sRtcProbeStatus = 1;
    else
    {
        if (sRtcProbeCode & 0xF0)
            sRtcProbeStatus = 2;
        else
            sRtcProbeStatus = 0;
        rtc_get_status_and_datetime(&sRtcInfoBuffer);
        sRtcProbeStatus = rtc_validate_datetime(&sRtcInfoBuffer);
    }
}

u16 rtc_get_probe_status(void)
{
    return sRtcProbeStatus;
}

void sub_020106EC(struct SiiRtcInfo * info)
{
    if (sRtcProbeStatus & 0xFF0)
        *info = sDefaultRTC;
    else
        rtc_get_status_and_datetime(info);
}

void rtc_get_datetime(struct SiiRtcInfo * info)
{
    rtc_intr_disable();
    SiiRtcGetDateTime(info);
    rtc_intr_enable();
}

void rtc_get_status(struct SiiRtcInfo * info)
{
    rtc_intr_disable();
    SiiRtcGetStatus(info);
    rtc_intr_enable();
}

void rtc_get_status_and_datetime(struct SiiRtcInfo * info)
{
    rtc_get_status(info);
    rtc_get_datetime(info);
}

u16 rtc_validate_datetime(struct SiiRtcInfo * info)
{
    u16 r4;
    s32 year, month, day;
    r4 = 0;
    if (info->status & SIIRTCINFO_POWER)
        r4 |= 0x20;
    if (!(info->status & SIIRTCINFO_24HOUR))
        r4 |= 0x10;
    year = bcd_to_hex(info->year);
    if (year == 0xFF)
        r4 |= 0x40;
    month = bcd_to_hex(info->month);
    if (month == 0xFF || month == 0 || month > 12)
        r4 |= 0x80;
    day = bcd_to_hex(info->day);
    if (day == 0xFF)
        r4 |= 0x100;
    if (month == MONTH_FEB)
    {
        if (day > is_leap_year(year) + sDaysPerMonth[1])
            r4 |= 0x100;
    }
    else
    {
        if (day > sDaysPerMonth[month - 1])
            r4 |= 0x100;
    }
    day = bcd_to_hex(info->hour);
    if (day == 0xFF || day > 24)
        r4 |= 0x200;
    day = bcd_to_hex(info->minute);
    if (day == 0xFF || day > 60)
        r4 |= 0x400;
    day = bcd_to_hex(info->second);
    if (day == 0xFF || day > 60)
        r4 |= 0x800;
    return r4;
}

void rtc_reset(void)
{
    rtc_intr_disable();
    SiiRtcReset();
    rtc_intr_enable();
}

void rtc_sub_time_from_datetime(struct SiiRtcInfo * datetime, struct Time * dest, struct Time * timediff)
{
    u16 r4 = rtc_count_days_from_info(datetime);
    dest->seconds = bcd_to_hex(datetime->second) - timediff->seconds;
    dest->minutes = bcd_to_hex(datetime->minute) - timediff->minutes;
    dest->hours = bcd_to_hex(datetime->hour) - timediff->hours;
    dest->days = r4 - timediff->days;
    if (dest->seconds < 0)
    {
        dest->seconds += 60;
        dest->minutes--;
    }
    if (dest->minutes < 0)
    {
        dest->minutes += 60;
        dest->hours--;
    }
    if (dest->hours < 0)
    {
        dest->hours += 24;
        dest->days--;
    }
}

void rtc_sub_time_from_time(struct Time * dest, struct Time * diff, struct Time * src)
{
    dest->seconds = src->seconds - diff->seconds;
    dest->minutes = src->minutes - diff->minutes;
    dest->hours = src->hours - diff->hours;
    dest->days = src->days - diff->days;
    if (dest->seconds < 0)
    {
        dest->seconds += 60;
        dest->minutes--;
    }
    if (dest->minutes < 0)
    {
        dest->minutes += 60;
        dest->hours--;
    }
    if (dest->hours < 0)
    {
        dest->hours += 24;
        dest->days--;
    }
}

void sub_0200F324(u8 unused)
{
    return;
}

void sub_0200F338(void)
{
    return;
}

void sub_0200F344(void)
{
    return;
}

u16 * bfix_var_get(u16 a0)
{
    if (a0 < VARS_START)
        return NULL;
    if (a0 < VAR_SPECIAL_0)
        return &gVarsPtr[a0 - VARS_START];
    return NULL;
}

bool32 check_pacifidlog_tm_received_day_before_today(void)
{
    u8 yearLo;
    u16 * data = bfix_var_get(VAR_PACIFIDLOG_TM_RECEIVED_DAY);
    rtc_maincb_is_time_since_last_berry_update_positive(&yearLo);
    if (*data <= gRtcUTCTime.days)
        return TRUE;
    else
        return FALSE;
}

bool32 bfix_fix_pacifidlog_tm(void)
{
    u16 * varAddr;
    u8 sp0;
    if (check_pacifidlog_tm_received_day_before_today() == TRUE)
        return TRUE;
    rtc_maincb_is_time_since_last_berry_update_positive(&sp0);
    if (gRtcUTCTime.days < 0)
        return FALSE;
    varAddr = bfix_var_get(VAR_PACIFIDLOG_TM_RECEIVED_DAY);
    *varAddr = 1;
    if (WriteSaveBlockChunks() != TRUE)
        return FALSE;
    return TRUE;
}
