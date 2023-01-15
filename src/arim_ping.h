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

#ifndef _ARIM_PING_H_INCLUDED_
#define _ARIM_PING_H_INCLUDED_

extern int arim_send_ping(const char *repeats, const char *to_call, int event);
extern int arim_send_ping_ack(void);
extern int arim_recv_ping(const char *data);
extern int arim_proc_ping(void);
extern int arim_recv_ping_ack(const char *data);
extern int arim_cancel_ping(void);

#endif

