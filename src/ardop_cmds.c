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
#include <ctype.h>
#include "main.h"
#include "arim.h"
#include "arim_beacon.h"
#include "arim_ping.h"
#include "arim_proto.h"
#include "arim_arq.h"
#include "arim_arq_files.h"
#include "arim_arq_msg.h"
#include "bufq.h"
#include "tnc_attach.h"
#include "ui.h"

size_t ardop_cmds_proc_resp(char *response, size_t size)
{
    static int negbw_once = 0;
    static char buffer[MAX_CMD_SIZE*3];
    char inbuffer[MAX_CMD_SIZE];
    static size_t cnt = 0;
    int quit = 0;
    char *end, *start, *val;

    if ((cnt + size) > sizeof(buffer)) {
        cnt = 0;
        return cnt;
    }
    memcpy(buffer + cnt, response, size);
    cnt += size;

    do {
        start = end = buffer;
        while (end < (buffer + cnt) && *end != '\r')
            ++end;
        if (end < (buffer + cnt) && *end == '\r') {
            *end = '\0';
            snprintf(inbuffer, sizeof(inbuffer), ">> %s", start);
            bufq_queue_cmd_in(inbuffer);
            bufq_queue_debug_log(inbuffer);
            /* process certain responses */
            val = start;
            while (*val && *val != ' ')
                ++val;
            if (*val)
                ++val;
            if (*val && !strncasecmp(val, "NOW ", 4)) {
                val += 4;
                while (*val && *val == ' ')
                    ++val;
            }
            if (!strncasecmp(start, "BUFFER", 6)) {
                pthread_mutex_lock(&mutex_tnc_set);
                snprintf(g_tnc_settings[g_cur_tnc].buffer,
                    sizeof(g_tnc_settings[g_cur_tnc].buffer), "%s", val);
                pthread_mutex_unlock(&mutex_tnc_set);
            } else if (!strncasecmp(start, "NEWSTATE", 8)) {
                pthread_mutex_lock(&mutex_tnc_set);
                snprintf(g_tnc_settings[g_cur_tnc].state,
                    sizeof(g_tnc_settings[g_cur_tnc].state), "%s", val);
                pthread_mutex_unlock(&mutex_tnc_set);
                arim_on_event(EV_TNC_NEWSTATE, 0);
            } else if (!strncasecmp(start, "CANCELPENDING", 13)) {
                arim_on_event(EV_ARQ_CAN_PENDING, 0);
            } else if (!strncasecmp(start, "PENDING", 7)) {
                arim_on_event(EV_ARQ_PENDING, 0);
            } else if (!strncasecmp(start, "DISCONNECTED", 12)) {
                arim_on_event(EV_ARQ_DISCONNECTED, 0);
            } else if (!strncasecmp(start, "CONNECTED", 9)) {
                /* parse remote call sign, ARQ bandwidth and grid square */
                start = end = val;
                while (*end && *end != ' ')
                    ++end;
                if (*end) {
                    *end = '\0';
                    ++end;
                }
                pthread_mutex_lock(&mutex_tnc_set);
                snprintf(g_tnc_settings[g_cur_tnc].arq_remote_call,
                    sizeof(g_tnc_settings[g_cur_tnc].arq_remote_call), "%s", start);
                /* parse ARQ bandwidth token */
                g_tnc_settings[g_cur_tnc].arq_bandwidth_hz[0] = '\0';
                if (*end) {
                    while (*end && *end == ' ')
                        ++end;
                    start = end;
                    while (*end && *end != ' ')
                        ++end;
                    if (*end) {
                        *end = '\0';
                        ++end;
                    }
                    snprintf(g_tnc_settings[g_cur_tnc].arq_bandwidth_hz,
                        sizeof(g_tnc_settings[g_cur_tnc].arq_bandwidth_hz), "%s", start);
                    if (g_tnc_version.major >= 2) {
                        /* parse grid square token (v2 TNC only) */
                        snprintf(g_tnc_settings[g_cur_tnc].arq_remote_gridsq,
                            sizeof(g_tnc_settings[g_cur_tnc].arq_remote_gridsq), "        ");
                        if (*end) {
                            while (*end && *end != '[')
                                ++end;
                            if (*end == '[') {
                                ++end;
                                start = end;
                                while (*end && *end != ']')
                                    ++end;
                                if (*end == ']') {
                                    *end = '\0';
                                    snprintf(g_tnc_settings[g_cur_tnc].arq_remote_gridsq,
                                        sizeof(g_tnc_settings[g_cur_tnc].arq_remote_gridsq), "%s", start);
                                }
                            }
                        }
                    }
                }
                pthread_mutex_unlock(&mutex_tnc_set);
                arim_on_event(EV_ARQ_CONNECTED, 0);
            } else if (!strncasecmp(start, "TARGET", 6)) {
                /* parse target call sign */
                pthread_mutex_lock(&mutex_tnc_set);
                snprintf(g_tnc_settings[g_cur_tnc].arq_target_call,
                    sizeof(g_tnc_settings[g_cur_tnc].arq_target_call), "%s", val);
                pthread_mutex_unlock(&mutex_tnc_set);
                arim_on_event(EV_ARQ_TARGET, 0);
            } else if (!strncasecmp(start, "REJECTEDBUSY", 12)) {
                pthread_mutex_lock(&mutex_tnc_set);
                snprintf(g_tnc_settings[g_cur_tnc].arq_remote_call,
                    sizeof(g_tnc_settings[g_cur_tnc].arq_remote_call), "%s", val);
                pthread_mutex_unlock(&mutex_tnc_set);
                arim_on_event(EV_ARQ_REJ_BUSY, 0);
            } else if (!strncasecmp(start, "REJECTEDBW", 10)) {
                pthread_mutex_lock(&mutex_tnc_set);
                snprintf(g_tnc_settings[g_cur_tnc].arq_remote_call,
                    sizeof(g_tnc_settings[g_cur_tnc].arq_remote_call), "%s", val);
                pthread_mutex_unlock(&mutex_tnc_set);
                arim_on_event(EV_ARQ_REJ_BW, 0);
            } else if (!strncasecmp(start, "LISTEN", 6)) {
                pthread_mutex_lock(&mutex_tnc_set);
                snprintf(g_tnc_settings[g_cur_tnc].tmp_listen,
                    sizeof(g_tnc_settings[g_cur_tnc].tmp_listen), "%s", val);
                pthread_mutex_unlock(&mutex_tnc_set);
                ui_set_title_dirty(TITLE_TNC_ATTACHED);
            } else if (!strncasecmp(start, "ENABLEPINGACK", 13)) {
                pthread_mutex_lock(&mutex_tnc_set);
                snprintf(g_tnc_settings[g_cur_tnc].en_pingack,
                    sizeof(g_tnc_settings[g_cur_tnc].en_pingack), "%s", val);
                pthread_mutex_unlock(&mutex_tnc_set);
                ui_set_title_dirty(TITLE_TNC_ATTACHED);
            } else if (!strncasecmp(start, "PINGACK", 7)) {
                arim_recv_ping_ack(start);
            } else if (!strncasecmp(start, "PINGREPLY", 9)) {
                arim_send_ping_ack();
            } else if (!strncasecmp(start, "PING", 4)) {
                arim_recv_ping(start);
            } else if (!strncasecmp(start, "PTT", 3)) {
                if (!strncasecmp(val, "TRUE", 4)) {
                    arim_on_event(EV_TNC_PTT, 1);
                    arim_beacon_reset_btimer();
                } else {
                    arim_on_event(EV_TNC_PTT, 0);
                }
            } else if (!strncasecmp(start, "STATE", 5)) {
                pthread_mutex_lock(&mutex_tnc_set);
                snprintf(g_tnc_settings[g_cur_tnc].state,
                    sizeof(g_tnc_settings[g_cur_tnc].state), "%s", val);
                pthread_mutex_unlock(&mutex_tnc_set);
            } else if (!strncasecmp(start, "FECMODE", 7)) {
                pthread_mutex_lock(&mutex_tnc_set);
                snprintf(g_tnc_settings[g_cur_tnc].fecmode,
                    sizeof(g_tnc_settings[g_cur_tnc].fecmode), "%s", val);
                pthread_mutex_unlock(&mutex_tnc_set);
                ui_set_status_dirty(STATUS_REFRESH);
            } else if (!strncasecmp(start, "FECREPEATS", 10)) {
                pthread_mutex_lock(&mutex_tnc_set);
                snprintf(g_tnc_settings[g_cur_tnc].fecrepeats,
                     sizeof(g_tnc_settings[g_cur_tnc].fecrepeats), "%s", val);
                pthread_mutex_unlock(&mutex_tnc_set);
                ui_set_status_dirty(STATUS_REFRESH);
            } else if (!strncasecmp(start, "FECID", 5)) {
                pthread_mutex_lock(&mutex_tnc_set);
                snprintf(g_tnc_settings[g_cur_tnc].fecid,
                     sizeof(g_tnc_settings[g_cur_tnc].fecid), "%s", val);
                pthread_mutex_unlock(&mutex_tnc_set);
            } else if (!strncasecmp(start, "MYCALL", 6)) {
                pthread_mutex_lock(&mutex_tnc_set);
                snprintf(g_tnc_settings[g_cur_tnc].mycall,
                     sizeof(g_tnc_settings[g_cur_tnc].mycall), "%s", val);
                pthread_mutex_unlock(&mutex_tnc_set);
                arim_beacon_set(-1);
            } else if (!strncasecmp(start, "GRIDSQUARE", 10)) {
                pthread_mutex_lock(&mutex_tnc_set);
                snprintf(g_tnc_settings[g_cur_tnc].gridsq,
                    sizeof(g_tnc_settings[g_cur_tnc].gridsq), "%s", val);
                pthread_mutex_unlock(&mutex_tnc_set);
                arim_beacon_set(-1);
            } else if (!strncasecmp(start, "SQUELCH", 7)) {
                pthread_mutex_lock(&mutex_tnc_set);
                snprintf(g_tnc_settings[g_cur_tnc].squelch,
                    sizeof(g_tnc_settings[g_cur_tnc].squelch), "%s", val);
                pthread_mutex_unlock(&mutex_tnc_set);
            } else if (!strncasecmp(start, "BUSYDET", 7)) {
                pthread_mutex_lock(&mutex_tnc_set);
                snprintf(g_tnc_settings[g_cur_tnc].busydet,
                    sizeof(g_tnc_settings[g_cur_tnc].busydet), "%s", val);
                pthread_mutex_unlock(&mutex_tnc_set);
            } else if (!strncasecmp(start, "BUSY", 4)) {
                if (!strncasecmp(val, "TRUE", 4)) {
                    pthread_mutex_lock(&mutex_tnc_set);
                    snprintf(g_tnc_settings[g_cur_tnc].busy,
                        sizeof(g_tnc_settings[g_cur_tnc].busy), "%s", "TRUE");
                    pthread_mutex_unlock(&mutex_tnc_set);
                    bufq_queue_debug_log("Cmd thread: TNC is BUSY");
                } else {
                    pthread_mutex_lock(&mutex_tnc_set);
                    snprintf(g_tnc_settings[g_cur_tnc].busy,
                        sizeof(g_tnc_settings[g_cur_tnc].busy), "%s", "FALSE");
                    pthread_mutex_unlock(&mutex_tnc_set);
                    bufq_queue_debug_log("Cmd thread: TNC is not BUSY");
                }
            } else if (!strncasecmp(start, "LEADER", 6)) {
                pthread_mutex_lock(&mutex_tnc_set);
                snprintf(g_tnc_settings[g_cur_tnc].leader,
                    sizeof(g_tnc_settings[g_cur_tnc].leader), "%s", val);
                pthread_mutex_unlock(&mutex_tnc_set);
            } else if (!strncasecmp(start, "TRAILER", 7)) {
                pthread_mutex_lock(&mutex_tnc_set);
                snprintf(g_tnc_settings[g_cur_tnc].trailer,
                    sizeof(g_tnc_settings[g_cur_tnc].trailer), "%s", val);
                pthread_mutex_unlock(&mutex_tnc_set);
            } else if (!strncasecmp(start, "ARQBW", 5)) {
                pthread_mutex_lock(&mutex_tnc_set);
                snprintf(g_tnc_settings[g_cur_tnc].arq_bandwidth,
                    sizeof(g_tnc_settings[g_cur_tnc].arq_bandwidth), "%s", val);
                pthread_mutex_unlock(&mutex_tnc_set);
            } else if (!strncasecmp(start, "VERSION", 7)) {
                pthread_mutex_lock(&mutex_tnc_set);
                snprintf(g_tnc_settings[g_cur_tnc].version,
                    sizeof(g_tnc_settings[g_cur_tnc].version), "%s", val);
                pthread_mutex_unlock(&mutex_tnc_set);
                tnc_get_version(val);
                /* NEGOTIATEBW command supported only by the ARDOP_2Win TNC */
                if (!negbw_once && g_tnc_version.vendor == 'W' &&
                                      g_tnc_version.major >= 2 &&
                                      g_tnc_version.minor >= 0 &&
                                      g_tnc_version.revision >= 4) {
                    negbw_once = 1;
                    snprintf(buffer, sizeof(buffer), "NEGOTIATEBW %s", g_tnc_settings[g_cur_tnc].arq_negotiate_bw);
                    bufq_queue_cmd_out(buffer);
                }
                /* now that we know tnc version, validate and update FECMODE */
                if (!ini_validate_fecmode(g_tnc_settings[g_cur_tnc].fecmode)) {
                    if (g_tnc_version.major <= 1)
                        snprintf(g_tnc_settings[g_cur_tnc].fecmode,
                                 sizeof(g_tnc_settings[g_cur_tnc].fecmode), "%s", "4FSK.200.50S");
                    else
                        snprintf(g_tnc_settings[g_cur_tnc].fecmode,
                                 sizeof(g_tnc_settings[g_cur_tnc].fecmode), "%s", "4PSK.200.50");
                }
                snprintf(buffer, sizeof(buffer), "FECMODE %s", g_tnc_settings[g_cur_tnc].fecmode);
                bufq_queue_cmd_out(buffer);
                /* next validate and update ARQBW */
                if (!ini_validate_arq_bw(g_tnc_settings[g_cur_tnc].arq_bandwidth)) {
                    if (g_tnc_version.major <= 1)
                        snprintf(g_tnc_settings[g_cur_tnc].arq_bandwidth,
                                 sizeof(g_tnc_settings[g_cur_tnc].arq_bandwidth), "%s", "500MAX");
                    else
                        snprintf(g_tnc_settings[g_cur_tnc].arq_bandwidth,
                                 sizeof(g_tnc_settings[g_cur_tnc].arq_bandwidth), "%s", "500");
                }
                snprintf(buffer, sizeof(buffer), "ARQBW %s", g_tnc_settings[g_cur_tnc].arq_bandwidth);
                bufq_queue_cmd_out(buffer);
            }
            cnt -= (end - buffer + 1);
            memmove(buffer, end + 1, cnt);
        } else {
            quit = 1;
        }
    } while (!quit);
    return cnt;
}

void ardop_cmds_init()
{
    int i;
    char buffer[MAX_CMD_SIZE];

    bufq_queue_cmd_out("INITIALIZE");
    snprintf(buffer, sizeof(buffer), "MYCALL %s", g_tnc_settings[g_cur_tnc].mycall);
    bufq_queue_cmd_out(buffer);
    snprintf(buffer, sizeof(buffer), "GRIDSQUARE %s", g_tnc_settings[g_cur_tnc].gridsq);
    bufq_queue_cmd_out(buffer);
    snprintf(buffer, sizeof(buffer), "FECID %s", g_tnc_settings[g_cur_tnc].fecid);
    bufq_queue_cmd_out(buffer);
    snprintf(buffer, sizeof(buffer), "FECREPEATS %s", g_tnc_settings[g_cur_tnc].fecrepeats);
    bufq_queue_cmd_out(buffer);
    snprintf(buffer, sizeof(buffer), "SQUELCH %s", g_tnc_settings[g_cur_tnc].squelch);
    bufq_queue_cmd_out(buffer);
    snprintf(buffer, sizeof(buffer), "BUSYDET %s", g_tnc_settings[g_cur_tnc].busydet);
    bufq_queue_cmd_out(buffer);
    snprintf(buffer, sizeof(buffer), "LEADER %s", g_tnc_settings[g_cur_tnc].leader);
    bufq_queue_cmd_out(buffer);
    snprintf(buffer, sizeof(buffer), "TRAILER %s", g_tnc_settings[g_cur_tnc].trailer);
    bufq_queue_cmd_out(buffer);
    snprintf(buffer, sizeof(buffer), "ENABLEPINGACK %s", g_tnc_settings[g_cur_tnc].en_pingack);
    bufq_queue_cmd_out(buffer);
    snprintf(buffer, sizeof(buffer), "ARQTIMEOUT %s", g_tnc_settings[g_cur_tnc].arq_timeout);
    bufq_queue_cmd_out(buffer);
    snprintf(buffer, sizeof(buffer), "LISTEN %s", g_tnc_settings[g_cur_tnc].listen);
    bufq_queue_cmd_out(buffer);
    bufq_queue_cmd_out("PROTOCOLMODE FEC");
    bufq_queue_cmd_out("AUTOBREAK TRUE");
    bufq_queue_cmd_out("MONITOR TRUE");
    bufq_queue_cmd_out("CWID FALSE");
    bufq_queue_cmd_out("VERSION");
    bufq_queue_cmd_out("STATE");
    /* send TNC initialization commands from config file */
    for (i = 0; i < g_tnc_settings[g_cur_tnc].tnc_init_cmds_cnt; i++)
        bufq_queue_cmd_out(g_tnc_settings[g_cur_tnc].tnc_init_cmds[i]);
}

