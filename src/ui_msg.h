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

#ifndef _UI_MSG_H_INCLUDED_
#define _UI_MSG_H_INCLUDED_

extern int msg_view_restart;

extern int ui_forward_msg(char *msgbuffer, size_t msgbufsize,const char *fn,
                              const char *hdr, const char *to_call);
extern int ui_send_msg(char *msgbuffer, size_t msgbufsize,
                           const char *fn, const char *hdr);
extern int ui_kill_msg(const char *fn, const char *hdr);
extern int ui_read_msg(const char *fn, const char *hdr,
                           int msgnbr, int is_recent);
extern void ui_list_msg(const char *fn, int mbox_type);
extern int ui_create_msg(char *buffer, size_t bufsize, const char *to);

#endif

