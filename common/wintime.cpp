/***
    This file is part of snapcast
    Copyright (C) 2014-2018  Johannes Pohl

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***/

#include "wintime.h"
#include <iostream>
#include <chrono>
#include <sysinfoapi.h>
#include <profileapi.h>

using namespace std;

// the following is lifted directly from
// https://stackoverflow.com/a/58162122

// Definition of a gettimeofday function

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    std::chrono::system_clock::duration d = std::chrono::system_clock::now().time_since_epoch();
    std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(d);
    tv->tv_sec = s.count();
    tv->tv_usec = std::chrono::duration_cast<std::chrono::microseconds>(d - s).count();

    return 0;
}

// lifted from
// https://stackoverflow.com/questions/5404277/porting-clock-gettime-to-windows
#define BILLION                             (1E9)

static BOOL g_first_time = 1;
static LARGE_INTEGER g_counts_per_sec;

int clock_gettime(int dummy, struct timespec *ct)
{
    LARGE_INTEGER count;

    if (g_first_time)
    {
        g_first_time = 0;

        if (0 == QueryPerformanceFrequency(&g_counts_per_sec))
        {
            g_counts_per_sec.QuadPart = 0;
        }
    }

    if ((NULL == ct) || (g_counts_per_sec.QuadPart <= 0) ||
            (0 == QueryPerformanceCounter(&count)))
    {
        return -1;
    }

    ct->tv_sec = count.QuadPart / g_counts_per_sec.QuadPart;
    ct->tv_nsec = ((count.QuadPart % g_counts_per_sec.QuadPart) * BILLION) / g_counts_per_sec.QuadPart;

    return 0;
}
