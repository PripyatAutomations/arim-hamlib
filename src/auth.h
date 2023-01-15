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

#ifndef _AUTH_H_INCLUDED_
#define _AUTH_H_INCLUDED_

/* to avoid padding in base64 strings size all digests as multiples of 3 */
#define AUTH_HA1_DIG_SIZE    30
#define AUTH_HA2_DIG_SIZE    30
#define AUTH_RESP_DIG_SIZE   21
#define AUTH_NONCE_SIZE      6
#define AUTH_MAX_PW_SIZE     32
#define AUTH_NONCE_B64_SIZE  ((AUTH_NONCE_SIZE/3)*4)
#define AUTH_RESP_B64_SIZE   ((AUTH_RESP_DIG_SIZE/3)*4)
#define AUTH_HA1_B64_SIZE    ((AUTH_HA1_DIG_SIZE/3)*4)
#define AUTH_HA2_B64_SIZE    ((AUTH_HA2_DIG_SIZE/3)*4)
#define AUTH_BUFFER_SIZE     (AUTH_HA1_B64_SIZE+AUTH_NONCE_B64_SIZE+AUTH_HA2_B64_SIZE+16)

extern char *auth_base64_encode(unsigned char *inbytes, size_t in_size,
                                char *outstr, size_t out_size);
extern char *auth_b64_digest(int digest_size, const unsigned char *inbytes,
                             size_t in_size, char *outstr, size_t out_size);
extern char *auth_b64_nonce(char *outstr, size_t out_size);
extern int auth_store_passwd(const char *remote_call, const char *local_call,
                             const char *password);
extern int auth_delete_passwd(const char *remote_call, const char *local_call);
extern int auth_check_passwd(const char *remote_call, const char *local_call,
                             char *ha1, size_t size);
extern int auth_init(void);
extern char g_arim_digest_fname[];

#endif

