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

#ifndef _CMDPROC_H_INCLUDED_
#define _CMDPROC_H_INCLUDED_

#define CMDPROC_OK            0
#define CMDPROC_FAIL        (-1)
#define CMDPROC_FILE_ERR    (-2)
#define CMDPROC_DIR_ERR     (-3)
#define CMDPROC_AUTH_REQ    (-4)
#define CMDPROC_AUTH_ERR    (-5)

extern int cmdproc_cmd(const char *cmd);
extern int cmdproc_query(const char *cmd, char *respbuf, size_t respbufsize);

#endif

