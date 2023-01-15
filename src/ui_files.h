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

#ifndef _UI_FILES_H_INCLUDED_
#define _UI_FILES_H_INCLUDED_

extern int ui_send_file(char *msgbuffer, int msgbufsize,
                const char *fn, const char *to_call);
extern int ui_get_file(const char *fn, char *filebuf, size_t filebufsize);
extern int ui_get_dyn_file(const char *fn, const char *cmd,
                                 char *filebuf, size_t filebufsize);
extern int ui_get_file_list(const char *basedir, const char *dir,
                                   char *listbuf, size_t listbufsize);
extern void ui_list_shared_files(void);
extern void ui_list_remote_files(const char *flist, const char *dir);

#endif

