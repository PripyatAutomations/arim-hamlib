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

#ifndef _ARIM_MESSAGE_H_INCLUDED_
#define _ARIM_MESSAGE_H_INCLUDED_

extern int arim_send_msg(const char *msg, const char *to);
extern int arim_send_msg_pp(void);
extern int arim_store_msg_prev_out(void);
extern int arim_store_msg_prev_sent(void);
extern int arim_recv_msg(const char *fm_call, const char *to_call,
                           unsigned int check, const char *msg);
extern int arim_cancel_msg(void);
extern void arim_recv_ack(const char *fm_call, const char *to_call);
extern void arim_recv_nak(const char *fm_call, const char *to_call);
extern size_t arim_msg_on_send_buffer(size_t size);

#endif

