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

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include "main.h"
#include "ui.h"
#include "bufq.h"
#include "arim_proto.h"
#include "arim_beacon.h"
#include "arim_query.h"
#include "arim_message.h"
#include "ini.h"
#include "log.h"
#include "util.h"

#define ST_NULL          0
#define ST_PIPE_1        1
#define ST_TYPE          2
#define ST_VERSION       3
#define ST_PIPE_2        4
#define ST_FROM_CALL     5
#define ST_PIPE_3        6
#define ST_TO_CALL       7
#define ST_PIPE_4        8
#define ST_SIZE          9
#define ST_GRIDSQ        10
#define ST_PIPE_5        11
#define ST_CHECK         12
#define ST_PIPE_6        13
#define ST_MSG           14
#define ST_BEACON_END    15
#define ST_MSG_END       16
#define ST_ACK_END       17
#define ST_NAK_END       18
#define ST_QUERY_END     19
#define ST_RESPONSE_END  20
#define ST_UNPROTO       21
#define ST_ERROR         99

//#define TRACE_PARSER

static char buffer[MAX_UNCOMP_DATA_SIZE];
static char to_call[TNC_MYCALL_SIZE];
static char fm_call[TNC_MYCALL_SIZE];
static char gridsq[TNC_GRIDSQ_SIZE];
static char *c;
static int type = 0, version = 0, check = 0;
static size_t cnt = 0, hdr_size = 0, msg_size = 0, msg_remaining = 0;
static int state = 0;

void arim_reset()
{
    c = buffer;
    type = version = cnt = 0;
    msg_size = msg_remaining = 0;
    memset(to_call, 0, sizeof(to_call));
    memset(fm_call, 0, sizeof(fm_call));
    memset(gridsq, 0, sizeof(gridsq));
    state = ST_PIPE_1;
    return;
}

int arim_test_frame(const char *data, size_t size)
{
    if (size >= 4 && data[0] == '|' &&
        (data[1] == 'M' || data[1] == 'Q' || data[1] == 'R' ||
         data[1] == 'B' || data[1] == 'A' || data[1] == 'N') &&
        (isdigit((int)data[2]) && isdigit((int)data[3]) && atoi(&data[2]) ==
        ARIM_PROTO_VERSION) && data[4] == '|') {
        return data[1];
    }
    return 0;
}

int arim_on_data(char *data, size_t size)
{
    int quit = 0, check_valid, numch, is_netcall, is_mycall;
    size_t remaining;
    char inbuffer[MIN_MSG_BUF_SIZE], numbuf[MAX_CHECK_SIZE];
    char *s, *e;

    if (!data || !size || (cnt + size) > MAX_UNCOMP_DATA_SIZE) {
        arim_reset();
        return 0; /* not waiting */
    }
    /* if a new frame arrives when waiting reset and start over */
    if (state != ST_PIPE_1 && arim_test_frame(data, size))
        arim_reset();
    memcpy(buffer + cnt, data, size);
    cnt += size;
    remaining = cnt - (c - buffer);
#ifdef TRACE_PARSER
snprintf(inbuffer, cnt, "%s", buffer);
bufq_queue_debug_log(inbuffer);
#endif

    do {
        switch (state) {
        case ST_PIPE_1:
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: entering pipe_1");
#endif
            c = buffer;
            if (*c++ == '|') {
                state = ST_TYPE;
                --remaining;
                hdr_size = 1;
            } else {
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: failed pipe_1");
snprintf(inbuffer, remaining, "%s", c);
bufq_queue_debug_log(inbuffer);
#endif
                quit = 1;
                state = ST_ERROR;
            }
            break;
        case ST_TYPE:
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: entering type");
#endif
            if (remaining >= 1) {
                if (*c == 'B' || *c == 'M' || *c == 'Q' ||
                    *c == 'R' || *c == 'A' || *c == 'N') {
                    type = *c;
                    c++;
                    --remaining;
                    ++hdr_size;
                    state = ST_VERSION;
#ifdef TRACE_PARSER
snprintf(inbuffer, sizeof(inbuffer), "type: %c", type);
bufq_queue_debug_log(inbuffer);
#endif
                } else {
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: failed type");
#endif
                    quit = 1;
                    state = ST_ERROR;
                }
            } else {
                quit = 1;
            }
            break;
        case ST_VERSION:
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: entering version");
#endif
            s = c;
            if (remaining >= 2 && isdigit((int)*c++) && isdigit((int)*c++)) {
                version = atoi(s);
                remaining -= 2;
                hdr_size += 2;
                if (version != ARIM_PROTO_VERSION) {
                    quit = 1;
                    state = ST_ERROR;
                } else {
                    state = ST_PIPE_2;
                }
#ifdef TRACE_PARSER
snprintf(inbuffer, sizeof(inbuffer), "version: %d", version);
bufq_queue_debug_log(inbuffer);
#endif
            } else {
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: failed version");
#endif
                quit = 1;
            }
            break;
        case ST_PIPE_2:
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: entering pipe_2");
#endif
            if (*c++ == '|') {
                --remaining;
                ++hdr_size;
                state = ST_FROM_CALL;
            } else {
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: failed pipe_2");
#endif
                quit = 1;
                state = ST_ERROR;
            }
            break;
        case ST_FROM_CALL:
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: entering from_call");
#endif
            e = c;
            while (*e && *e != '|' && (e - c) < remaining)
                ++e;
            if (*e == '|') {
                strncpy(fm_call, c, e - c);
                fm_call[e - c + 1] = '\0';
                remaining -= (e - c);
                hdr_size += (e - c);
                c = e;
                state = ST_PIPE_3;
#ifdef TRACE_PARSER
snprintf(inbuffer, sizeof(inbuffer), "from_call: %s", fm_call);
bufq_queue_debug_log(inbuffer);
#endif
            } else {
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: failed from_call");
#endif
                quit = 1;
            }
           break;
        case ST_PIPE_3:
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: entering pipe_3");
#endif
            if (*c++ == '|') {
                --remaining;
                ++hdr_size;
                if (type == 'B')
                    state = ST_SIZE;
                else
                    state = ST_TO_CALL;
            } else {
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: failed pipe_3");
#endif
                quit = 1;
                state = ST_ERROR;
            }
            break;
        case ST_TO_CALL:
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: entering to_call");
#endif
            e = c;
            while (*e && *e != '|' && (e - c) < remaining)
                ++e;
            if (*e == '|') {
                strncpy(to_call, c, e - c);
                to_call[e - c + 1] = '\0';
                remaining -= (e - c);
                hdr_size += (e - c);
                c = e;
                state = ST_PIPE_4;
#ifdef TRACE_PARSER
snprintf(inbuffer, sizeof(inbuffer), "to_call: %s", to_call);
bufq_queue_debug_log(inbuffer);
#endif
            } else {
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: failed to_call");
#endif
                quit = 1;
            }
            break;
        case ST_SIZE:
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: entering size");
#endif
            s = c;
            if (remaining >= 4 && isxdigit((int)*c++) && isxdigit((int)*c++) &&
                                  isxdigit((int)*c++) && isxdigit((int)*c++)) {
                numbuf[0] = s[0];
                numbuf[1] = s[1];
                numbuf[2] = s[2];
                numbuf[3] = s[3];
                numbuf[4] = '\0';
                if (1 == sscanf(numbuf, "%zx", &msg_size) && msg_size < MIN_MSG_BUF_SIZE) {
                    remaining -= 4;
                    hdr_size += 4;
                    if (type == 'M' || type == 'R') {
                        is_mycall = arim_test_mycall(to_call);
                        is_netcall = arim_test_netcall(to_call);
                        if (is_mycall || is_netcall) {
                            /* start the download progress meter */
                            ui_status_xfer_start(0, msg_size, STATUS_XFER_DIR_DOWN);
                        }
                    }
                    if (type == 'M' || type == 'Q' || type == 'R')
                        state = ST_PIPE_5;
                    else if (type == 'B')
                        state = ST_PIPE_4;
#ifdef TRACE_PARSER
snprintf(inbuffer, sizeof(inbuffer), "msg_size: %04X", msg_size);
bufq_queue_debug_log(inbuffer);
#endif
                } else {
                    quit = 1;
                    state = ST_ERROR;
                }
            } else {
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: failed size");
#endif
                quit = 1;
            }
            break;
        case ST_PIPE_4:
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: entering pipe_4");
#endif
            if (*c++ == '|') {
                --remaining;
                ++hdr_size;
                if (type == 'M' || type == 'Q' || type == 'R')
                    state = ST_SIZE;
                else if (type == 'A') {
                    *c = '\0';
                    quit = 1;
                    state = ST_ACK_END;
                }
                else if (type == 'N') {
                    *c = '\0';
                    quit = 1;
                    state = ST_NAK_END;
                }
                else if (type == 'B')
                    state = ST_GRIDSQ;
            } else {
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: failed pipe_4");
#endif
                quit = 1;
                state = ST_ERROR;
            }
            break;
        case ST_GRIDSQ:
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: entering gridsq");
#endif
            e = c;
            while (*e && *e != '|' && (e - c) < remaining)
                ++e;
            if (*e == '|') {
                strncpy(gridsq, c, e - c);
                gridsq[e - c + 1] = '\0';
                remaining -= (e - c);
                hdr_size += (e - c);
                c = e;
                state = ST_PIPE_5;
#ifdef TRACE_PARSER
snprintf(inbuffer, sizeof(inbuffer), "gridsq: %s", gridsq);
bufq_queue_debug_log(inbuffer);
#endif
            } else {
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: failed gridsq");
#endif
                quit = 1;
            }
            break;
        case ST_PIPE_5:
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: entering pipe_5");
#endif
            if (*c++ == '|') {
                --remaining;
                ++hdr_size;
                if (type == 'M' || type == 'Q' || type == 'R')
                    state = ST_CHECK;
                else if (type == 'B') {
                    msg_remaining = msg_size - hdr_size;
                    state = ST_MSG;
                }
            } else {
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: failed pipe_5");
#endif
                quit = 1;
                state = ST_ERROR;
            }
            break;
        case ST_CHECK:
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: entering check");
#endif
            s = c;
            if (remaining >= 4 && isxdigit((int)*c++) && isxdigit((int)*c++) &&
                                  isxdigit((int)*c++) && isxdigit((int)*c++)) {
                numbuf[0] = s[0];
                numbuf[1] = s[1];
                numbuf[2] = s[2];
                numbuf[3] = s[3];
                numbuf[4] = '\0';
                if (1 == sscanf(numbuf, "%x", &check)) {
                    remaining -= 4;
                    hdr_size += 4;
                    state = ST_PIPE_6;
#ifdef TRACE_PARSER
snprintf(inbuffer, sizeof(inbuffer), "check: %04X", check);
bufq_queue_debug_log(inbuffer);
#endif
                } else {
                    quit = 1;
                    state = ST_ERROR;
                }
            } else {
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: failed check");
#endif
                quit = 1;
            }
            break;
        case ST_PIPE_6:
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: entering pipe_6");
#endif
            if (*c++ == '|') {
                --remaining;
                ++hdr_size;
                msg_remaining = msg_size - hdr_size;
                state = ST_MSG;
            } else {
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: failed pipe_6");
#endif
                quit = 1;
                state = ST_ERROR;
            }
            break;
        case ST_MSG:
#ifdef TRACE_PARSER
bufq_queue_debug_log("Parser: entering msg");
#endif
            if (remaining && remaining <= msg_remaining) {
                msg_remaining -= remaining;
                c += remaining;
                remaining = 0;
            }
            else if (remaining && remaining > msg_remaining) {
                c += msg_remaining;
                remaining -= msg_remaining;
                msg_remaining = 0;
            }
            if (!msg_remaining) {
                buffer[msg_size] = '\0';
                if (type == 'M')
                    state = ST_MSG_END;
                else if (type == 'Q')
                    state = ST_QUERY_END;
                else if (type == 'R')
                    state = ST_RESPONSE_END;
                else if (type == 'B')
                    state = ST_BEACON_END;
#ifdef TRACE_PARSER
snprintf(inbuffer, sizeof(inbuffer), "%s", buffer + hdr_size);
bufq_queue_debug_log(inbuffer);
sleep(1);
#endif
                quit = 1;
            }
            break;
        default: /* illegal state */
            arim_reset();
            quit = 1;
            break;
        } /* end switch */
    } while (!quit && remaining > 0);
    if (state == ST_MSG_END) {
        if (!ini_check_ac_calls(fm_call)) {
            numch = snprintf(inbuffer, sizeof(inbuffer), ">> [%c] (Access denied) %s", 'M', buffer);
            if (numch >= sizeof(inbuffer))
                ui_truncate_line(inbuffer, sizeof(inbuffer));
            bufq_queue_traffic_log(inbuffer);
            numch = snprintf(inbuffer, sizeof(inbuffer), ">> [%c] Ignored [M] frame from %s (access denied)", 'X', fm_call);
            if (numch >= sizeof(inbuffer))
                ui_truncate_line(inbuffer, sizeof(inbuffer));
            bufq_queue_data_in(inbuffer);
            bufq_queue_debug_log("Data thread: ignored ARIM [M] frame from TNC (access denied)");
        } else {
            check_valid = arim_recv_msg(fm_call, to_call, check, buffer + hdr_size);
            numch = snprintf(inbuffer, sizeof(inbuffer), ">> [%c] %s", check_valid ? 'M' : '!', buffer);
            if (numch >= sizeof(inbuffer))
                ui_truncate_line(inbuffer, sizeof(inbuffer));
            bufq_queue_traffic_log(inbuffer);
            bufq_queue_data_in(inbuffer);
            bufq_queue_debug_log("Data thread: received ARIM [M] frame from TNC");
        }
        arim_reset();
        /* end the download progress meter */
        ui_status_xfer_end();
    } else if (state == ST_BEACON_END) {
        numch = snprintf(inbuffer, sizeof(inbuffer), ">> [B] %s", buffer);
        if (numch >= sizeof(inbuffer))
            ui_truncate_line(inbuffer, sizeof(inbuffer));
        bufq_queue_data_in(inbuffer);
        bufq_queue_traffic_log(inbuffer);
        bufq_queue_debug_log("Data thread: received ARIM [B] frame from TNC");
        arim_beacon_recv(fm_call, gridsq, buffer + hdr_size);
        arim_reset();
    } else if (state == ST_QUERY_END) {
        if (!ini_check_ac_calls(fm_call)) {
            numch = snprintf(inbuffer, sizeof(inbuffer), ">> [%c] (access denied) %s", 'Q', buffer);
            if (numch >= sizeof(inbuffer))
                ui_truncate_line(inbuffer, sizeof(inbuffer));
            bufq_queue_traffic_log(inbuffer);
            numch = snprintf(inbuffer, sizeof(inbuffer), ">> [%c] Ignored [Q] frame from %s (access denied)", 'X', fm_call);
            if (numch >= sizeof(inbuffer))
                ui_truncate_line(inbuffer, sizeof(inbuffer));
            bufq_queue_data_in(inbuffer);
            bufq_queue_debug_log("Data thread: ignored ARIM [Q] frame from TNC (access denied)");
        } else {
            check_valid = arim_recv_query(fm_call, to_call, check, buffer + hdr_size);
            numch = snprintf(inbuffer, sizeof(inbuffer), ">> [%c] %s", check_valid ? 'Q' : '!', buffer);
            if (numch >= sizeof(inbuffer))
                ui_truncate_line(inbuffer, sizeof(inbuffer));
            bufq_queue_data_in(inbuffer);
            bufq_queue_traffic_log(inbuffer);
            bufq_queue_debug_log("Data thread: received ARIM [Q] frame from TNC");
        }
        arim_reset();
    } else if (state == ST_RESPONSE_END) {
        check_valid = arim_recv_response(fm_call, to_call, check, buffer + hdr_size);
        numch = snprintf(inbuffer, sizeof(inbuffer), ">> [%c] %s", check_valid ? 'R' : '!', buffer);
        if (numch >= sizeof(inbuffer))
            ui_truncate_line(inbuffer, sizeof(inbuffer));
        bufq_queue_data_in(inbuffer);
        bufq_queue_traffic_log(inbuffer);
        bufq_queue_debug_log("Data thread: received ARIM [R] frame from TNC");
        arim_reset();
        /* end the download progress meter */
        ui_status_xfer_end();
    } else if (state == ST_ACK_END) {
        msg_size = 7 + strlen(fm_call) + strlen(to_call);
        buffer[msg_size] = 0;
        numch = snprintf(inbuffer, sizeof(inbuffer), ">> [A] %s", buffer);
        if (numch >= sizeof(inbuffer))
            ui_truncate_line(inbuffer, sizeof(inbuffer));
        bufq_queue_data_in(inbuffer);
        bufq_queue_traffic_log(inbuffer);
        bufq_queue_debug_log("Data thread: received ARIM [A] frame from TNC");
        arim_recv_ack(fm_call, to_call);
        arim_reset();
    } else if (state == ST_NAK_END) {
        msg_size = 7 + strlen(fm_call) + strlen(to_call);
        buffer[msg_size] = 0;
        numch = snprintf(inbuffer, sizeof(inbuffer), ">> [N] %s", buffer);
        if (numch >= sizeof(inbuffer))
            ui_truncate_line(inbuffer, sizeof(inbuffer));
        bufq_queue_data_in(inbuffer);
        bufq_queue_traffic_log(inbuffer);
        bufq_queue_debug_log("Data thread: received ARIM [N] frame from TNC");
        arim_recv_nak(fm_call, to_call);
        arim_reset();
    } else if (state == ST_ERROR) {
        buffer[remaining] = '\0';
        numch = snprintf(inbuffer, sizeof(inbuffer), ">> [!] %s", buffer);
        if (numch >= sizeof(inbuffer))
            ui_truncate_line(inbuffer, sizeof(inbuffer));
        bufq_queue_data_in(inbuffer);
        bufq_queue_traffic_log(inbuffer);
        bufq_queue_debug_log("Data thread: received ARIM [!] frame from TNC");
        arim_reset();
        /* end the download progress meter */
        ui_status_xfer_end();
    } else if (type == 'M' || type == 'R') {
        /* update the download progress meter */
        ui_status_xfer_update(cnt);
    }
    if (state == ST_PIPE_1)
        return 0; /* not waiting */
    else
        return 1; /* waiting */
}

