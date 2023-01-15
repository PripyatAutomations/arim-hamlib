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

#ifndef _ARIM_ARQ_AUTH_H_INCLUDED_
#define _ARIM_ARQ_AUTH_H_INCLUDED_

extern void arim_arq_auth_set_ha2_info(const char *method, const char *path);
extern void arim_arq_auth_set_status(int val);
extern int arim_arq_auth_get_status(void);
extern int arim_arq_auth_on_send_a1(const char *call, const char *method, const char *path);
extern int arim_arq_auth_on_send_a2(void);
extern int arim_arq_auth_on_send_a3(void);
extern int arim_arq_auth_on_a1(char *cmd, size_t size, char *eol);
extern int arim_arq_auth_on_a2(char *cmd, size_t size, char *eol);
extern int arim_arq_auth_on_a3(char *cmd, size_t size, char *eol);
extern void arim_arq_auth_on_ok(void);
extern void arim_arq_auth_on_error(void);
extern int arim_arq_auth_on_challenge(char *cmd, size_t size, char *eol);
extern int arim_arq_auth_on_client_challenge(const char *cmd);

#endif

