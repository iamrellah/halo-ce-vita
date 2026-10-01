#include "halo_vita_file_handle.h"
#include <psp2/rtc.h>
#include <string.h>
#include <stdlib.h>

LONG WINAPI CompareFileTime(const FILETIME *left,const FILETIME *right)
{
    uint64_t a=((uint64_t)left->dwHighDateTime<<32)|left->dwLowDateTime;
    uint64_t b=((uint64_t)right->dwHighDateTime<<32)|right->dwLowDateTime;
    return a<b ? -1 : a>b ? 1 : 0;
}

BOOL WINAPI SystemTimeToFileTime(const SYSTEMTIME *input,FILETIME *output)
{
    SceDateTime time;
    SceUInt64 value;
    if (!input || !output || input->wMilliseconds>999 || input->wYear<1601) {
        SetLastError(ERROR_INVALID_PARAMETER);return FALSE;
    }
    memset(&time,0,sizeof(time));
    time.year=input->wYear;time.month=input->wMonth;time.day=input->wDay;
    time.hour=input->wHour;time.minute=input->wMinute;time.second=input->wSecond;
    time.microsecond=(unsigned)input->wMilliseconds*1000;
    /* wDayOfWeek is intentionally ignored, as in the Windows API. */
    if (sceRtcCheckValid(&time)<0 || sceRtcGetWin32FileTime(&time,&value)<0) {
        SetLastError(ERROR_INVALID_PARAMETER);return FALSE;
    }
    output->dwLowDateTime=(DWORD)value;output->dwHighDateTime=(DWORD)(value>>32);
    return TRUE;
}

void WINAPI GetSystemTime(SYSTEMTIME *output)
{
    SceDateTime time;
    SYSTEMTIME result;
    int weekday;
    /* Win32 has no error return; fail instead of publishing a fabricated date. */
    if (!output || sceRtcGetCurrentClock(&time,0)<0 || sceRtcCheckValid(&time)<0) abort();
    weekday=sceRtcGetDayOfWeek(time.year,time.month,time.day);
    if (weekday<0 || weekday>6) abort();
    memset(&result,0,sizeof(result));
    result.wYear=time.year;result.wMonth=time.month;result.wDay=time.day;
    result.wDayOfWeek=(unsigned short)weekday;
    result.wHour=time.hour;result.wMinute=time.minute;result.wSecond=time.second;
    result.wMilliseconds=(unsigned short)(time.microsecond/1000);
    *output=result;
}
