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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include "main.h"
#include "ini.h"
#include "util.h"
#include "blake2.h"
#include "auth.h"
#include "ui_dialog.h"

#define MAX_PASSWD_LINE_SIZE  (AUTH_HA1_B64_SIZE+MAX_CALLSIGN_SIZE+MAX_CALLSIGN_SIZE+16)

char g_arim_digest_fname[MAX_PATH_SIZE];
static char arim_digest_dir_path[MAX_DIR_PATH_SIZE];

char *auth_base64_encode(unsigned char *inbytes, size_t in_size, char *outstr, size_t out_size)
{
    int num_triplets = in_size / 3;
    int remainder = in_size % 3;
    unsigned char c1, c2, c3, *in = inbytes;
    unsigned char enc_array[64];
    char *out = outstr;
    int i, val;

    if ((num_triplets * 4) + 5 > out_size)
        return NULL;

    for (i = 0; i < 64; i++)
    {
        if (i < 26)
            val = i + 'A';
        else if (i < 52)
            val = (i - 26) + 'a';
        else if (i < 62)
            val = (i - 52) + '0';
        else if (i == 62)
            val = '+';
        else
            val = '/';

        enc_array[i] = (unsigned char)val;
    }
    /* encode triplets */
    for (i = 0; i < num_triplets; i++)
    {
        c1 = in[0];
        c2 = in[1];
        c3 = in[2];

        *out++ = enc_array[(c1 >> 2)];
        *out++ = enc_array[((c1 & 3) << 4) | (c2 >> 4)];
        *out++ = enc_array[((c2 & 15) << 2) | (c3 >> 6)];
        *out++ = enc_array[(c3 & 63)];

        in += 3;
    }
    /* deal with remainders */
    if (remainder == 2)
    {
        c1 = in[0];
        c2 = in[1];

        *out++ = enc_array[(c1 >> 2)];
        *out++ = enc_array[((c1 & 3) << 4) | (c2 >> 4)];
        *out++ = enc_array[((c2 & 15) << 2)];
        *out++ = '=';
    }
    else if (remainder == 1)
    {
        c1 = in[0];

        *out++ = enc_array[(c1 >> 2)];
        *out++ = enc_array[((c1 & 3) << 4)];
        *out++ = '=';
        *out++ = '=';
    }
    /* null terminate output string */
    *out = '\0';
    /* done */
    return outstr;
}

char *auth_b64_digest(int digest_size, const unsigned char *inbytes,
                      size_t in_size, char *outstr, size_t out_size)
{
    uint8_t digest[BLAKE2S_OUTBYTES];

    blake2s(digest, digest_size, inbytes, in_size, NULL, 0);
    if (NULL == auth_base64_encode(digest, digest_size, outstr, out_size)) {
        outstr[0] = '\0';
        return NULL;
    }
    return outstr;
}

char *auth_b64_nonce(char *outstr, size_t out_size)
{
    uint8_t digest[BLAKE2S_OUTBYTES];
    time_t t;

    t = time(NULL);
    blake2s(digest, AUTH_NONCE_SIZE, &t, sizeof(t), NULL, 0);
    if (NULL == auth_base64_encode(digest, AUTH_NONCE_SIZE, outstr, out_size)) {
        outstr[0] = '\0';
        return NULL;
    }
    return outstr;
}

int auth_init()
{
    FILE *tempfp;
    int result;

    snprintf(arim_digest_dir_path, sizeof(arim_digest_dir_path), "%s", g_arim_path);
    snprintf(g_arim_digest_fname, sizeof(g_arim_digest_fname), "%s/%s",
             arim_digest_dir_path, DEFAULT_DIGEST_FNAME);
    result = access(g_arim_digest_fname, F_OK);
    if (result != 0) {
        if (errno == ENOENT) {
            tempfp = fopen(g_arim_digest_fname, "a");
            if (tempfp == NULL) {
                return 0;
            } else {
                fclose(tempfp);
            }
        } else {
            return 0;
        }
    }
    return 1;
}

int auth_store_passwd(const char *remote_call, const char *local_call, const char *password)
{
    FILE *arim_digest_fp, *tempfp;
    int fd, numch;
    char *p, linebuf[MAX_PASSWD_LINE_SIZE], entry[MAX_PASSWD_LINE_SIZE];
    char ha1[AUTH_BUFFER_SIZE], tempfn[MAX_PATH_SIZE];
    char lcall[TNC_MYCALL_SIZE], rcall[TNC_MYCALL_SIZE];
    size_t i, len;

    /* open files */
    arim_digest_fp = fopen(g_arim_digest_fname, "r");
    if (arim_digest_fp == NULL)
        return 0;
    snprintf(tempfn, sizeof(tempfn), "%s/temp.%s.XXXXXX",
                  arim_digest_dir_path, DEFAULT_DIGEST_FNAME);
    fd = mkstemp(tempfn);
    if (fd == -1) {
        fclose(arim_digest_fp);
        return 0;
    }
    tempfp = fdopen(fd, "r+");
    if (tempfp == NULL) {
        close(fd);
        fclose(arim_digest_fp);
        return 0;
    }
    flockfile(arim_digest_fp);
    /* force calls to uppercase */
    snprintf(rcall, sizeof(rcall), "%s", remote_call);
    len = strlen(rcall);
    for (i = 0; i < len; i++)
        rcall[i] = toupper(rcall[i]);
    snprintf(lcall, sizeof(lcall), "%s", local_call);
    len = strlen(lcall);
    for (i = 0; i < len; i++)
        lcall[i] = toupper(lcall[i]);
    /* create arim-digest file line */
    snprintf(linebuf, sizeof(linebuf), "%s:%s:%s", rcall, lcall, password);
    auth_b64_digest(AUTH_HA1_DIG_SIZE, (const unsigned char *)linebuf,
                       strlen(linebuf), ha1, sizeof(ha1));
    numch = snprintf(entry, sizeof(entry), "%s:%s:%s\n", rcall, lcall, ha1);
    len = strlen(rcall) + strlen(lcall) + 2;
    /* find matching entry in file */
    p = fgets(linebuf, sizeof(linebuf), arim_digest_fp);
    while (p && strncmp(linebuf, entry, len)) {
        /* write into temp file */
        fprintf(tempfp, "%s", linebuf);
        p = fgets(linebuf, sizeof(linebuf), arim_digest_fp);
    }
    if (p) {
        /* got it, replace with new entry */
        fprintf(tempfp, "%s", entry);
        p = fgets(linebuf, sizeof(linebuf), arim_digest_fp);
        while (p) {
            /* write into temp file */
            fprintf(tempfp, "%s", linebuf);
            p = fgets(linebuf, sizeof(linebuf), arim_digest_fp);
        }
    } else {
        /* new entry, write to temp file */
        fprintf(tempfp, "%s", entry);
    }
    funlockfile(arim_digest_fp);
    fclose(arim_digest_fp);
    unlink(g_arim_digest_fname);
    fclose(tempfp);
    rename(tempfn, g_arim_digest_fname);
    (void)numch; /* suppress 'assigned but not used' warning for dummy var */
    return 1;
}

int auth_delete_passwd(const char *remote_call, const char *local_call)
{
    FILE *arim_digest_fp, *tempfp;
    int fd;
    char *p, linebuf[MAX_PASSWD_LINE_SIZE], entry[MAX_PASSWD_LINE_SIZE];
    char tempfn[MAX_PATH_SIZE];
    char lcall[TNC_MYCALL_SIZE], rcall[TNC_MYCALL_SIZE];
    size_t i, len;

    /* open files */
    arim_digest_fp = fopen(g_arim_digest_fname, "r");
    if (arim_digest_fp == NULL)
        return 0;
    snprintf(tempfn, sizeof(tempfn), "%s/temp.%s.XXXXXX",
                  arim_digest_dir_path, DEFAULT_DIGEST_FNAME);
    fd = mkstemp(tempfn);
    if (fd == -1) {
        fclose(arim_digest_fp);
        return 0;
    }
    tempfp = fdopen(fd, "r+");
    if (tempfp == NULL) {
        close(fd);
        fclose(arim_digest_fp);
        return 0;
    }
    flockfile(arim_digest_fp);
    /* force calls to uppercase */
    snprintf(rcall, sizeof(rcall), "%s", remote_call);
    len = strlen(rcall);
    for (i = 0; i < len; i++)
        rcall[i] = toupper(rcall[i]);
    snprintf(lcall, sizeof(lcall), "%s", local_call);
    len = strlen(lcall);
    for (i = 0; i < len; i++)
        lcall[i] = toupper(lcall[i]);
    /* create arim-digest file line */
    snprintf(entry, sizeof(entry), "%s:%s:", rcall, lcall);
    len = strlen(entry);
    /* find matching entry in file */
    p = fgets(linebuf, sizeof(linebuf), arim_digest_fp);
    while (p && strncmp(linebuf, entry, len)) {
        /* write into temp file */
        fprintf(tempfp, "%s", linebuf);
        p = fgets(linebuf, sizeof(linebuf), arim_digest_fp);
    }
    if (p) {
        /* got it, skip over */
        p = fgets(linebuf, sizeof(linebuf), arim_digest_fp);
        while (p) {
            /* write into temp file */
            fprintf(tempfp, "%s", linebuf);
            p = fgets(linebuf, sizeof(linebuf), arim_digest_fp);
        }
    }
    funlockfile(arim_digest_fp);
    fclose(arim_digest_fp);
    unlink(g_arim_digest_fname);
    fclose(tempfp);
    rename(tempfn, g_arim_digest_fname);
    return 1;
}

int auth_check_passwd(const char *remote_call, const char *local_call, char *ha1, size_t size)
{
    FILE *arim_digest_fp;
    char *p, linebuf[MAX_PASSWD_LINE_SIZE], entry[MAX_PASSWD_LINE_SIZE];
    char lcall[TNC_MYCALL_SIZE], rcall[TNC_MYCALL_SIZE];
    size_t i, len, found = 0;

    /* open files */
    arim_digest_fp = fopen(g_arim_digest_fname, "r");
    if (arim_digest_fp == NULL)
        return 0;
    flockfile(arim_digest_fp);
    /* force calls to uppercase */
    snprintf(rcall, sizeof(rcall), "%s", remote_call);
    len = strlen(rcall);
    for (i = 0; i < len; i++)
        rcall[i] = toupper(rcall[i]);
    snprintf(lcall, sizeof(lcall), "%s", local_call);
    len = strlen(lcall);
    for (i = 0; i < len; i++)
        lcall[i] = toupper(lcall[i]);
    /* create arim-digest file line */
    snprintf(entry, sizeof(entry), "%s:%s:", rcall, lcall);
    len = strlen(entry);
    /* find matching entry in file */
    p = fgets(linebuf, sizeof(linebuf), arim_digest_fp);
    while (p && strncmp(linebuf, entry, len)) {
        p = fgets(linebuf, sizeof(linebuf), arim_digest_fp);
    }
    if (p) {
        /* got it, extract HA1 */
        snprintf(ha1, size, "%s", p + len);
        /* remove newline character */
        len = strlen(ha1);
        ha1[len - 1] = '\0';
        found = 1;
    }
    funlockfile(arim_digest_fp);
    fclose(arim_digest_fp);
    return found;
}

