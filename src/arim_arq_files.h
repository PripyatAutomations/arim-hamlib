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

#ifndef _ARIM_ARQ_FILES_H_INCLUDED_
#define _ARIM_ARQ_FILES_H_INCLUDED_

extern int arim_arq_files_on_send_cmd(void);
extern int arim_arq_files_on_fput(char *cmd, size_t size, char *eol, int arq_cs_role);
extern int arim_arq_files_on_fget(char *cmd, size_t size, char *eol);
extern int arim_arq_files_on_flput(char *cmd, size_t size, char *eol);
extern int arim_arq_files_on_flget(char *cmd, size_t size, char *eol);
extern int arim_arq_files_on_client_fget(const char *cmd, const char *fn, const char *destdir, int use_zoption);
extern int arim_arq_files_on_client_fput(const char *fn, const char *destdir, int use_zoption);
extern int arim_arq_files_on_client_flget(const char *cmd, const char *destdir, int use_zoption);
extern int arim_arq_files_on_client_flist(const char *cmd);
extern int arim_arq_files_on_client_file(const char *cmd);
extern int arim_arq_files_send_file(const char *fn, const char *destdir, int is_local);
extern size_t arim_arq_files_on_send_buffer(size_t size);
extern int arim_arq_files_on_rcv_frame(const char *data, size_t size);
extern int arim_arq_files_flist_on_rcv_frame(const char *data, size_t size);
extern int arim_arq_files_flist_on_send_cmd(void);
extern size_t arim_arq_files_flist_on_send_buffer(size_t size);
extern void arim_arq_files_on_flget_done(void);

#endif

