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

#ifndef _MBOX_H_INCLUDED_
#define _MBOX_H_INCLUDED_

#include "main.h"

extern int mbox_init(void);
extern char *mbox_add_msg(const char *fn, const char *fm_call, const char *to_call,
                              int check, const char *msg, int trace);
extern int mbox_delete_msg(const char *fn, const char *hdr);
extern int mbox_save_msg(const char *fn, const char *hdr, const char *savefn);
extern int mbox_set_flag(const char *fn, const char *hdr, int flag);
extern int mbox_clear_flag(const char *fn, const char *hdr, int flag);
extern int mbox_read_msg(char *msgbuffer, size_t msgbufsize,
                             const char *fn, const char *hdr);
extern int mbox_fwd_msg(char *msgbuffer, size_t msgbufsize,
                            const char *fn, const char *hdr);
extern int mbox_send_msg(char *msgbuffer, size_t msgbufsize, char *tocall,
                             size_t to_call_size, const char *fn, const char *hdr);
extern int mbox_get_msg_list(char *msgbuffer, size_t msgbufsize,
                                 const char *fn, const char *to_call);
extern int mbox_get_headers_to(char headers[][MAX_MBOX_HDR_SIZE],
                        int max_hdrs, const char *fn, const char *to_call);
extern int mbox_get_msg(char *msgbuffer, size_t msgbufsize,
                            const char *fn, const char *hdr, int canonical_eol);
extern int mbox_purge(const char *fn, int days);
extern char mbox_dir_path[];

#endif

