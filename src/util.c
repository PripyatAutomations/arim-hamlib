/***********************************************************************

    ARIM Amateur Radio Instant Messaging program for the ARDOP TNC.

    Copyright (C) 2016-2021 Robert Cunnings NW8L

    This file is part of the ARIM messaging program.

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

*************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "main.h"
#include "ini.h"
#include "util.h"

const char *days[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
};

const char *months[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

unsigned int ccitt_crc16(const unsigned char *data, size_t size)
{
    size_t i, cnt;
    unsigned int work, cs;

    cs = 0xFFFF;
    if (size < 1)
        return ~cs;
    cnt = 0;
    do {
        work = 0x00FF & data[cnt++];
        for (i = 0; i < 8; i++) {
            if ((cs & 0x0001) ^ (work & 0x0001))
                cs = (cs >> 1) ^ 0x8408;
            else
                cs >>= 1;
            work >>= 1;
        }
    } while (cnt < size);
    cs = ~cs;
    work = cs;
    cs = (cs << 8) | (work >> 8 & 0x00FF);
    return cs & 0xFFFF;
}

char *util_timestamp(char *buffer, size_t maxsize)
{
    time_t t;
    struct tm *cur_time;

    pthread_mutex_lock(&mutex_time);
    t = time(NULL);
    if (!strncasecmp(g_ui_settings.utc_time, "TRUE", 4))
        cur_time = gmtime(&t);
    else
        cur_time = localtime(&t);
    snprintf(buffer, maxsize, "%02d:%02d:%02d",
                cur_time->tm_hour, cur_time->tm_min, cur_time->tm_sec);
    pthread_mutex_unlock(&mutex_time);
    return buffer;
}

char *util_timestamp_usec(char *buffer, size_t maxsize)
{
    struct timeval tv;
    struct tm *cur_time;

    pthread_mutex_lock(&mutex_time);
    gettimeofday(&tv, NULL);
    if (!strncasecmp(g_ui_settings.utc_time, "TRUE", 4))
        cur_time = gmtime(&tv.tv_sec);
    else
        cur_time = localtime(&tv.tv_sec);
    snprintf(buffer, maxsize, "%02d:%02d:%02d.%06ld",
                cur_time->tm_hour, cur_time->tm_min, cur_time->tm_sec, tv.tv_usec);
    pthread_mutex_unlock(&mutex_time);
    return buffer;
}

char *util_datestamp(char *buffer, size_t maxsize)
{
    time_t t;
    struct tm *cur_time;

    pthread_mutex_lock(&mutex_time);
    t = time(NULL);
    if (!strncasecmp(g_ui_settings.utc_time, "TRUE", 4))
        cur_time = gmtime(&t);
    else
        cur_time = localtime(&t);
    snprintf(buffer, maxsize, "2%03d%02d%02d", cur_time->tm_year - 100,
                cur_time->tm_mon + 1, cur_time->tm_mday);
    pthread_mutex_unlock(&mutex_time);
    return buffer;
}

char *util_date_timestamp(char *buffer, size_t maxsize)
{
    time_t t;
    struct tm *cur_time;

    pthread_mutex_lock(&mutex_time);
    t = time(NULL);
    if (!strncasecmp(g_ui_settings.utc_time, "TRUE", 4))
        cur_time = gmtime(&t);
    else
        cur_time = localtime(&t);
    snprintf(buffer, maxsize, "%s %s %2d %02d:%02d:%02d 2%03d",
                days[cur_time->tm_wday], months[cur_time->tm_mon],
                    cur_time->tm_mday, cur_time->tm_hour, cur_time->tm_min,
                        cur_time->tm_sec, cur_time->tm_year - 100);
    pthread_mutex_unlock(&mutex_time);
    return buffer;
}

char *util_rcv_timestamp(char *buffer, size_t maxsize)
{
    time_t t;
    struct tm *cur_time;

    pthread_mutex_lock(&mutex_time);
    t = time(NULL);
    if (!strncasecmp(g_ui_settings.utc_time, "TRUE", 4)) {
        cur_time = gmtime(&t);
        snprintf(buffer, maxsize, "%s %2d 2%03d %02d:%02d:%02d UTC",
                    months[cur_time->tm_mon], cur_time->tm_mday, cur_time->tm_year - 100,
                        cur_time->tm_hour, cur_time->tm_min, cur_time->tm_sec);
    } else {
        cur_time = localtime(&t);
        snprintf(buffer, maxsize, "%s %2d 2%03d %02d:%02d:%02d",
                    months[cur_time->tm_mon], cur_time->tm_mday, cur_time->tm_year - 100,
                        cur_time->tm_hour, cur_time->tm_min, cur_time->tm_sec);
    }
    pthread_mutex_unlock(&mutex_time);
    return buffer;
}

char *util_file_timestamp(time_t t, char *buffer, size_t maxsize)
{
    struct tm *cur_time;

    pthread_mutex_lock(&mutex_time);
    if (!strncasecmp(g_ui_settings.utc_time, "TRUE", 4))
        cur_time = gmtime(&t);
    else
        cur_time = localtime(&t);
    snprintf(buffer, maxsize, "%s %2d %02d:%02d 2%03d",
                months[cur_time->tm_mon], cur_time->tm_mday,
                    cur_time->tm_hour, cur_time->tm_min, cur_time->tm_year - 100);
    pthread_mutex_unlock(&mutex_time);
    return buffer;
}

char *util_clock(char *buffer, size_t maxsize)
{
    time_t t;
    struct tm *cur_time;

    pthread_mutex_lock(&mutex_time);
    t = time(NULL);
    if (!strncasecmp(g_ui_settings.utc_time, "TRUE", 4))
        cur_time = gmtime(&t);
    else
        cur_time = localtime(&t);
    snprintf(buffer, maxsize, "%s %02d %02d:%02d", months[cur_time->tm_mon],
                cur_time->tm_mday, cur_time->tm_hour, cur_time->tm_min);
    pthread_mutex_unlock(&mutex_time);
    return buffer;
}

char *util_clock_tm(time_t t, char *buffer, size_t maxsize)
{
    struct tm *cur_time;

    pthread_mutex_lock(&mutex_time);
    if (!strncasecmp(g_ui_settings.utc_time, "TRUE", 4))
        cur_time = gmtime(&t);
    else
        cur_time = localtime(&t);
    snprintf(buffer, maxsize, "%s %02d %02d:%02d:%02d", months[cur_time->tm_mon],
                cur_time->tm_mday, cur_time->tm_hour, cur_time->tm_min, cur_time->tm_sec);
    pthread_mutex_unlock(&mutex_time);
    return buffer;
}

