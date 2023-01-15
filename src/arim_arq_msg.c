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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include "main.h"
#include "bufq.h"
#include "arim_proto.h"
#include "ui.h"
#include "util.h"
#include "ui_dialog.h"
#include "ui_tnc_data_win.h"
#include "log.h"
#include "mbox.h"
#include "zlib.h"
#include "datathread.h"
#include "arim_arq.h"
#include "arim_arq_auth.h"
#include "arim_arq_msg.h"
#include "auth.h"

static MSGQUEUEITEM msg_in;
static MSGQUEUEITEM msg_out;
static size_t msg_in_cnt, msg_out_cnt;
static char headers[MAX_MGET_HEADERS][MAX_MBOX_HDR_SIZE];
static int zoption, num_msgs, next_msg, send_done;

int arim_arq_msg_on_send_cmd(const char *data, int use_zoption)
{
    char linebuf[MAX_LOG_LINE_SIZE];
    z_stream zs;
    int zret;

    zoption = use_zoption;
    /* copy into buffer, will be sent later by arim_arq_msg_on_send_cmd() */
    if (zoption) {
        zs.zalloc = Z_NULL;
        zs.zfree = Z_NULL;
        zs.opaque = Z_NULL;
        zs.avail_in = strlen(data);
        zs.next_in = (Bytef *)data;
        zs.avail_out = sizeof(msg_out.data);
        zs.next_out = (Bytef *)msg_out.data;
        zret = deflateInit(&zs, Z_BEST_COMPRESSION);
        if (zret == Z_OK) {
            zret = deflate(&zs, Z_FINISH);
            deflateEnd(&zs);
            if (zret != Z_STREAM_END) {
                ui_show_dialog("\tCannot send message:\n"
                               "\tcompression failed.\n \n\t[O]k", "oO \n");
                snprintf(linebuf, sizeof(linebuf),
                                "ARQ: Message upload failed, compression error");
                bufq_queue_debug_log(linebuf);
                return 0;
            }
            msg_out.size = zs.total_out;
        } else {
            ui_show_dialog("\tCannot send message:\n"
                           "\tcompression failed.\n \n\t[O]k", "oO \n");
            snprintf(linebuf, sizeof(linebuf),
                            "ARQ: Message upload failed, compression init error");
            bufq_queue_debug_log(linebuf);
            return 0;
        }
    } else {
        snprintf(msg_out.data, sizeof(msg_out.data), "%s", data);
        msg_out.size = strlen(msg_out.data);
    }
    msg_out.check = ccitt_crc16((unsigned char *)msg_out.data, msg_out.size);
    arim_copy_remote_call(msg_out.call, sizeof(msg_out.call));
    /* enqueue command for TNC */
    snprintf(linebuf, sizeof(linebuf),
        "%s %s %zu %04X", zoption ? "/MPUT -z" : "/MPUT",
            msg_out.call, msg_out.size, msg_out.check);
    arim_arq_send_remote(linebuf);
    /* initialize count and start progress meter */
    msg_out_cnt = 0;
    ui_status_xfer_start(0, msg_out.size, STATUS_XFER_DIR_UP);
    /* change state to begin transfer */
    arim_on_event(EV_ARQ_MSG_SEND_CMD, 0);
    return 1;
}

int arim_arq_msg_on_send_msg()
{
    char linebuf[MAX_LOG_LINE_SIZE];

    pthread_mutex_lock(&mutex_msg_out);
    msgq_push(&g_msg_out_q, &msg_out);
    pthread_mutex_unlock(&mutex_msg_out);
    snprintf(linebuf, sizeof(linebuf), "ARQ: Message upload buffered for sending");
    bufq_queue_debug_log(linebuf);
    send_done = 0;
    return 1;
}

size_t arim_arq_msg_on_send_buffer(size_t size)
{
    static int prev_size = -1, prev_msg_out_cnt = 0;
    char linebuf[MAX_LOG_LINE_SIZE];
    size_t msg_out_buffered;

    if (!send_done) {
        if (prev_size == -1 && size == 0)
            return 1; /* wait for nonzero buffer count (size) */
        if (prev_size == size)
            return 1; /* ignore repeated BUFFER count */
        msg_out_buffered = datathread_get_num_bytes_buffered();
        if (msg_out_buffered >= size)
            msg_out_cnt = msg_out_buffered - size;
        else
            return 1; /* must be non-negative number of bytes */
        prev_size = size;
        if (msg_out_cnt == 0 || msg_out_cnt == prev_msg_out_cnt)
            return 1; /* don't double-print upload status lines */
        prev_msg_out_cnt = msg_out_cnt;
        snprintf(linebuf, sizeof(linebuf),
            "ARQ: Message to %s sending %zu of %zu bytes",
                msg_out.call, msg_out_cnt, msg_out.size);
        bufq_queue_debug_log(linebuf);
        snprintf(linebuf, sizeof(linebuf), "<< [@] Message to %s %zu of %zu bytes",
                    msg_out.call, msg_out_cnt, msg_out.size);
        bufq_queue_traffic_log(linebuf);
        bufq_queue_data_in(linebuf);
        /* update progress meter */
        ui_status_xfer_update(msg_out_cnt);
        /* if done, re-arm for next upload */
        if (msg_out_cnt == msg_out.size) {
            send_done = 1;
            prev_size = -1;
            prev_msg_out_cnt = 0;
            return 0;
        }
    }
    return 1;
}

int arim_arq_msg_on_rcv_frame(const char *data, size_t size)
{
    char remote_call[TNC_MYCALL_SIZE], target_call[TNC_MYCALL_SIZE];
    char *hdr, linebuf[MAX_LOG_LINE_SIZE];
    unsigned int check;
    z_stream zs;
    char zbuffer[MAX_UNCOMP_DATA_SIZE];
    int zret;

    /* buffer data, increment count of bytes */
    if (msg_in_cnt + size > sizeof(msg_in.data)) {
        /* overflow */
        snprintf(linebuf, sizeof(linebuf),
            "ARQ: Message download failed, buffer overflow %zu",
                msg_in_cnt + size);
        bufq_queue_debug_log(linebuf);
        snprintf(linebuf, sizeof(linebuf), "/ERROR Message buffer overflow");
        arim_arq_send_remote(linebuf);
        arim_on_event(EV_ARQ_MSG_ERROR, 0);
        return 0;
    }
    memcpy(msg_in.data + msg_in_cnt, data, size);
    msg_in_cnt += size;
    msg_in.data[msg_in_cnt] = '\0';
    snprintf(linebuf, sizeof(linebuf),
        "ARQ: Message download reading %zu of %zu bytes", msg_in_cnt, msg_in.size);
    bufq_queue_debug_log(linebuf);
    arim_copy_remote_call(remote_call, sizeof(remote_call));
    snprintf(linebuf, sizeof(linebuf),
        ">> [@] Message from %s %zu of %zu bytes",
            remote_call, msg_in_cnt, msg_in.size);
    bufq_queue_traffic_log(linebuf);
    bufq_queue_data_in(linebuf);
    /* update progress meter */
    ui_status_xfer_update(msg_in_cnt);
    arim_on_event(EV_ARQ_MSG_RCV_FRAME, 0);
    if (msg_in_cnt >= msg_in.size) {
        /* if excess data, take most recent msg_in_size bytes */
        if (msg_in_cnt > msg_in.size) {
            memmove(msg_in.data, msg_in.data + (msg_in_cnt - msg_in.size),
                                                msg_in_cnt - msg_in.size);
            msg_in_cnt = msg_in.size;
        }
        /* verify checksum */
        check = ccitt_crc16((unsigned char *)msg_in.data, msg_in.size);
        if (msg_in.check != check) {
            snprintf(linebuf, sizeof(linebuf),
               "ARQ: Message download failed, bad checksum %04X",  check);
            bufq_queue_debug_log(linebuf);
            snprintf(linebuf, sizeof(linebuf), "/ERROR Bad checksum");
            arim_arq_send_remote(linebuf);
            arim_on_event(EV_ARQ_MSG_ERROR, 0);
            return 0;
        }
        if (zoption) {
            zs.zalloc = Z_NULL;
            zs.zfree = Z_NULL;
            zs.opaque = Z_NULL;
            zs.avail_in = msg_in.size;
            zs.next_in = (Bytef *)msg_in.data;
            zs.avail_out = sizeof(zbuffer) - 1; /* leave room for null termination */
            zs.next_out = (Bytef *)zbuffer;
            zret = inflateInit(&zs);
            if (zret == Z_OK) {
                zret = inflate(&zs, Z_FINISH);
                inflateEnd(&zs);
                if (zret != Z_STREAM_END) {
                    snprintf(linebuf, sizeof(linebuf),
                        "ARQ: Message download failed, decompression error");
                    bufq_queue_debug_log(linebuf);
                    snprintf(linebuf, sizeof(linebuf), "/ERROR Message size exceeds limit");
                    arim_arq_send_remote(linebuf);
                    arim_on_event(EV_ARQ_MSG_ERROR, 0);
                    return 0;
                }
            } else {
                snprintf(linebuf, sizeof(linebuf),
                    "ARQ: Message download failed, decompression initialization error");
                bufq_queue_debug_log(linebuf);
                snprintf(linebuf, sizeof(linebuf), "/ERROR Decompression failed");
                arim_arq_send_remote(linebuf);
                arim_on_event(EV_ARQ_MSG_ERROR, 0);
                return 0;
            }
            msg_in.size = zs.total_out;
            zbuffer[msg_in.size] = '\0'; /* restore terminating null */
            msg_in.check = ccitt_crc16((unsigned char *)zbuffer, msg_in.size);
        }
        /* now store message to inbox */
        arim_copy_mycall(target_call, sizeof(target_call));
        if (!zoption)
            hdr = mbox_add_msg(MBOX_INBOX_FNAME, remote_call, target_call, msg_in.check, msg_in.data, 1);
        else
            hdr = mbox_add_msg(MBOX_INBOX_FNAME, remote_call, target_call, msg_in.check, zbuffer, 1);
        if (hdr == NULL) {
            snprintf(linebuf, sizeof(linebuf),
                "ARQ: Message download failed, could not open inbox");
            bufq_queue_debug_log(linebuf);
            snprintf(linebuf, sizeof(linebuf), "/ERROR Unable to save message");
            arim_arq_send_remote(linebuf);
            arim_on_event(EV_ARQ_MSG_ERROR, 0);
            return 0;
        }
        /* add message header to recents list */
        pthread_mutex_lock(&mutex_recents);
        cmdq_push(&g_recents_q, hdr);
        pthread_mutex_unlock(&mutex_recents);
        snprintf(linebuf, sizeof(linebuf),
            "ARQ: Saved %s message %zu bytes, checksum %04X",
               zoption ? "compressed" : "uncompressed",  msg_in_cnt, check);
        bufq_queue_debug_log(linebuf);
        snprintf(linebuf, sizeof(linebuf),
            "/OK Message %zu %04X saved", msg_in_cnt, check);
        arim_arq_send_remote(linebuf);
        arim_on_event(EV_ARQ_MSG_RCV_DONE, 0);
    }
    return 1;
}

int arim_arq_msg_on_send_first(const char *remote_call, int max_msgs)
{
    char linebuf[MAX_LOG_LINE_SIZE], msgbuffer[MAX_UNCOMP_DATA_SIZE];

    next_msg = 0;
    if (max_msgs > MAX_MGET_HEADERS || max_msgs == 0)
        max_msgs = MAX_MGET_HEADERS;
    /* get up to max_msgs message headers To: remote_call */
    num_msgs = mbox_get_headers_to(headers, max_msgs,
                                       MBOX_OUTBOX_FNAME, remote_call);
    if (!num_msgs)
        return 0;
    if (mbox_get_msg(msgbuffer, sizeof(msgbuffer),
                        MBOX_OUTBOX_FNAME, headers[next_msg], 0)) {
        snprintf(linebuf, sizeof(linebuf),
            "ARQ: Sending message %d of %d, [%s]",
                next_msg + 1, num_msgs, headers[next_msg]);
        bufq_queue_debug_log(linebuf);
        arim_arq_msg_on_send_cmd(msgbuffer, zoption);
        return 1;
    } else {
        snprintf(linebuf, sizeof(linebuf), "/ERROR Cannot find message");
        arim_arq_send_remote(linebuf);
        snprintf(linebuf, sizeof(linebuf),
            "ARQ: Failed to read message %d of %d, %s",
                next_msg, num_msgs, headers[next_msg]);
        bufq_queue_debug_log(linebuf);
    }
    /* failed, reset counters */
    num_msgs = next_msg = 0;
    return 0;
}

int arim_arq_msg_on_send_next()
{
    char linebuf[MAX_LOG_LINE_SIZE], msgbuffer[MAX_UNCOMP_DATA_SIZE];

    /* send next message if available */
    if (next_msg < num_msgs) {
        /* delete previous message from mbox */
        if (!mbox_delete_msg(MBOX_OUTBOX_FNAME, headers[next_msg])) {
            /* log, but don't stop if deletion fails */
            snprintf(linebuf, sizeof(linebuf),
                "ARQ: Failed to delete message %d of %d, %s", next_msg, num_msgs, headers[next_msg]);
            bufq_queue_debug_log(linebuf);
        }
        ++next_msg;
        if (next_msg < num_msgs) {
            if (mbox_get_msg(msgbuffer, sizeof(msgbuffer),
                        MBOX_OUTBOX_FNAME, headers[next_msg], 0)) {
                snprintf(linebuf, sizeof(linebuf),
                    "ARQ: Sending message %d of %d, [%s]", next_msg + 1, num_msgs, headers[next_msg]);
                bufq_queue_debug_log(linebuf);
                arim_arq_msg_on_send_cmd(msgbuffer, zoption);
                return 1;
            } else {
                /* failed to read message, send /ERROR response */
                snprintf(linebuf, sizeof(linebuf), "/ERROR Cannot find message");
                arim_arq_send_remote(linebuf);
                snprintf(linebuf, sizeof(linebuf),
                    "ARQ: Failed to read message %d of %d, %s", next_msg, num_msgs, headers[next_msg]);
                bufq_queue_debug_log(linebuf);
            }
        } else {
            /* all done */
            snprintf(linebuf, sizeof(linebuf),
                "/OK Done, %d of %d messages", next_msg, num_msgs);
            arim_arq_send_remote(linebuf);
        }
    }
    /* done, reset counters */
    num_msgs = next_msg = 0;
    return 0;
}

int arim_arq_msg_on_mget(char *cmd, size_t size, char *eol)
{
    char *p_args, *s, *e;
    char linebuf[MAX_LOG_LINE_SIZE], remote_call[TNC_MYCALL_SIZE];
    int result, num_msgs = 0;

    zoption = 0;
    p_args = NULL;
    /* empty outbound data buffer before handling file request */
    while (arim_get_buffer_cnt() > 0)
        sleep(1);
    /* parse the parameters */
    s = cmd + 6;
    while (*s && *s == ' ')
        ++s;
    if (*s && (s == strstr(s, "-z"))) {
        zoption = 1;
        s += 2;
        while (*s && *s == ' ')
            ++s;
    }
    p_args = s;
    if (*p_args && eol) {
        /* trim trailing spaces */
        e = eol - 1;
        while (e > p_args && *e == ' ') {
            *e = '\0';
            --e;
        }
        num_msgs = atoi(p_args);
    }
    arim_copy_remote_call(remote_call, sizeof(remote_call));
    if (!arim_arq_auth_get_status()) {
        /* auth required, send /A1 challenge */
        if (arim_arq_auth_on_send_a1(remote_call, "MGET", p_args)) {
            arim_on_event(EV_ARQ_AUTH_SEND_CMD, 1);
        } else {
            /* no access for remote call, send /EAUTH response */
            snprintf(linebuf, sizeof(linebuf), "/EAUTH");
            arim_arq_send_remote(linebuf);
        }
    } else {
        /* no auth required or session previously authenticated */
        result = arim_arq_msg_on_send_first(remote_call, num_msgs);
        if (!result) {
            snprintf(linebuf, sizeof(linebuf),
                "/OK No messages for %s", remote_call);
            arim_arq_send_remote(linebuf);
        }
    }
    return 1;
}

int arim_arq_msg_on_mput(char *cmd, size_t size, char *eol)
{
    char *p_check, *p_name, *p_size, *e;
    char linebuf[MAX_LOG_LINE_SIZE];

    zoption = 0;
    /* inbound message transfer, get parameters */
    p_size = p_check = 0;
    e = cmd + 6;
    while (*e && *e == ' ')
        ++e;
    if (*e && (e == strstr(e, "-z"))) {
        zoption = 1;
        e += 2;
        while (*e && *e == ' ')
            ++e;
    }
    p_name = e;
    if (*p_name) {
        while (*e && *e != ' ') {
            ++e;
        }
        /* at end of call sign */
        if (*e) {
            *e = '\0';
            ++e;
            /* at start of size */
            p_size = e;
            while (*e && *e != ' ') {
                ++e;
            }
            *e = '\0';
            ++e;
            /* at start of check */
            p_check = e;
        }
        if (p_size && p_check) {
            arim_on_event(EV_ARQ_MSG_RCV, 0);
            snprintf(msg_in.call, sizeof(msg_in.call), "%s", p_name);
            msg_in.size = atoi(p_size);
            if (1 != sscanf(p_check, "%x", &msg_in.check))
                msg_in.check = 0;
            snprintf(linebuf, sizeof(linebuf),
                        "ARQ: Message download %s %zu %04X started",
                           msg_in.call , msg_in.size, msg_in.check);
            bufq_queue_debug_log(linebuf);
            /* initialize count and start progress meter */
            msg_in_cnt = 0;
            ui_status_xfer_start(0, msg_in.size, STATUS_XFER_DIR_DOWN);
            /* cache any data remaining */
            if ((cmd + size) > eol) {
                arim_arq_msg_on_rcv_frame(eol, size - (eol - cmd));
            }
        } else {
            snprintf(linebuf, sizeof(linebuf),
                "ARQ: Message download %s failed, bad size/checksum parameter", p_name);
            bufq_queue_debug_log(linebuf);
            snprintf(linebuf, sizeof(linebuf), "/ERROR Bad parameters");
            arim_arq_send_remote(linebuf);
            arim_on_event(EV_ARQ_MSG_ERROR, 0);
        }
    } else {
        snprintf(linebuf, sizeof(linebuf),
                    "ARQ: Message download failed, bad /MPUT call sign");
        bufq_queue_debug_log(linebuf);
        snprintf(linebuf, sizeof(linebuf), "/ERROR Bad call sign");
        arim_arq_send_remote(linebuf);
        arim_on_event(EV_ARQ_MSG_ERROR, 0);
    }
    return 1;
}

int arim_arq_msg_on_ok()
{
    char linebuf[MAX_LOG_LINE_SIZE];
    z_stream zs;
    char zbuffer[MAX_UNCOMP_DATA_SIZE];
    int zret;

    if (zoption) {
        zs.zalloc = Z_NULL;
        zs.zfree = Z_NULL;
        zs.opaque = Z_NULL;
        zs.avail_in = msg_out.size;
        zs.next_in = (Bytef *)msg_out.data;
        zs.avail_out = sizeof(zbuffer);
        zs.next_out = (Bytef *)zbuffer;
        zret = inflateInit(&zs);
        if (zret == Z_OK) {
            zret = inflate(&zs, Z_FINISH);
            inflateEnd(&zs);
            if (zret != Z_STREAM_END) {
                snprintf(linebuf, sizeof(linebuf),
                    "ARQ: Unable to save sent message, decompression failed");
                bufq_queue_debug_log(linebuf);
                return 0;
            } else {
                msg_out.size = zs.total_out;
                zbuffer[msg_out.size] = '\0';
                msg_out.check = ccitt_crc16((unsigned char *)zbuffer, msg_out.size);
            }
        } else {
            snprintf(linebuf, sizeof(linebuf),
                "ARQ: Unable to save sent message, decompression init error");
            bufq_queue_debug_log(linebuf);
            return 0;
        }
    }
    /* store message to sent messages mailbox */
    arim_copy_mycall(linebuf, sizeof(linebuf));
    if (!zoption)
        mbox_add_msg(MBOX_SENTBOX_FNAME,
            linebuf, msg_out.call, msg_out.check, msg_out.data, 0);
    else
        mbox_add_msg(MBOX_SENTBOX_FNAME,
            linebuf, msg_out.call, msg_out.check, zbuffer, 0);
    return 1;
}

int arim_arq_msg_on_mlist(char *cmdbuf, size_t cmdbufsize, char *eol,
                              char *respbuf, size_t respbufsize)
{
    char linebuf[MAX_LOG_LINE_SIZE], remote_call[TNC_MYCALL_SIZE];
    size_t len, i;

    arim_copy_remote_call(remote_call, sizeof(remote_call));
    len = strlen(remote_call);
    for (i = 0; i < len; i++)
        remote_call[i] = toupper(remote_call[i]);
    /* check to see if authentication is needed */
    if (!arim_arq_auth_get_status()) {
        /* auth required, send /A1 challenge */
        if (arim_arq_auth_on_send_a1(remote_call, "MLIST", "")) {
            arim_on_event(EV_ARQ_AUTH_SEND_CMD, 1);
            snprintf(linebuf, sizeof(linebuf),
                        "ARQ: Listing of msgs for %s requires authentication", remote_call);
            bufq_queue_debug_log(linebuf);
        } else {
            /* no access for remote call, send /EAUTH response */
            snprintf(linebuf, sizeof(linebuf), "/EAUTH");
            arim_arq_send_remote(linebuf);
            snprintf(linebuf, sizeof(linebuf),
                    "ARQ: Listing of msgs no password for: %s", remote_call);
            bufq_queue_debug_log(linebuf);
        }
    } else {
        if (mbox_get_msg_list(respbuf, respbufsize, MBOX_OUTBOX_FNAME, remote_call)) {
            return 1;
        } else {
            /* failed to get list, send /ERROR response */
            snprintf(linebuf, sizeof(linebuf), "/ERROR Cannot list messages");
            arim_arq_send_remote(linebuf);
            snprintf(linebuf, sizeof(linebuf),
                    "ARQ: Listing of msgs failed for: %s", remote_call);
            bufq_queue_debug_log(linebuf);
        }
    }
    return 0;
}

int arim_arq_msg_on_client_mlist(const char *cmd)
{
    char *s, *e;
    char salt[MAX_MBOX_HDR_SIZE], linebuf[MAX_LOG_LINE_SIZE];
    char mycall[TNC_MYCALL_SIZE], remote_call[TNC_MYCALL_SIZE];
    static char ha1[AUTH_BUFFER_SIZE];

    /* retrieve HA1 from password file */
    arim_copy_mycall(mycall, sizeof(mycall));
    arim_copy_remote_call(remote_call, sizeof(remote_call));
    if (!auth_check_passwd(mycall, remote_call, ha1, sizeof(ha1))) {
        snprintf(linebuf, sizeof(linebuf),
            "AUTH: No entry for call %s in arim-digest file)", remote_call);
        bufq_queue_debug_log(linebuf);
        ui_set_status_dirty(STATUS_ARQ_EAUTH_REMOTE);
        return 0;
    }
    /* called from cmd processor when user issues /AUTH at prompt */
    snprintf(salt, sizeof(salt), "%s", cmd);
    s = salt + 6;
    while (*s && *s == ' ')
        ++s;
    if (*s) {
        /* trim trailing spaces */
        e = s + strlen(s) - 1;
        while (e > s && *e == ' ') {
            *e = '\0';
            --e;
        }
    }
    /* cache the command for re-sending when auth is done */
    arim_arq_cache_cmd(cmd);
    arim_arq_auth_set_ha2_info("MLIST", s);
    arim_arq_send_remote(cmd);
    return 1;
}

int arim_arq_msg_on_client_mget(const char *cmd, const char *args, int use_zoption)
{
    /* called from cmd processor when user issues /MGET at prompt */
    char *s, *e, argbuf[MAX_CMD_SIZE];
    size_t len;

    argbuf[0] = '\0';
    s = argbuf;
    if (args) {
        snprintf(argbuf, sizeof(argbuf), "%s", args);
        /* trim leading and trailing spaces */
        while (*s && *s == ' ')
            ++s;
        len = strlen(argbuf);
        e = &argbuf[len - 1];
        while (e > s && *e == ' ') {
            *e = '\0';
            --e;
        }
    }
    arim_arq_auth_set_ha2_info("MGET", s);
    arim_arq_send_remote(cmd);
    return 1;
}

