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

#ifndef _UTIL_H_INCLUDED_
#define _UTIL_H_INCLUDED_

#define MAX_TIMESTAMP_SIZE   32

extern char *util_timestamp(char *buffer, size_t maxsize);
extern char *util_timestamp_usec(char *buffer, size_t maxsize);
extern char *util_datestamp(char *buffer, size_t maxsize);
extern char *util_date_timestamp(char *buffer, size_t maxsize);
extern char *util_file_timestamp(time_t t, char *buffer, size_t maxsize);
extern char *util_rcv_timestamp(char *buffer, size_t maxsize);
extern char *util_clock(char *buffer, size_t maxsize);
extern char *util_clock_tm(time_t t, char *buffer, size_t maxsize);
extern unsigned int ccitt_crc16(const unsigned char *data, size_t size);

#endif

