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
#include <ctype.h>
#include <string.h>
#include "main.h"
#include "arim_beacon.h"
#include "arim_message.h"
#include "arim_proto.h"
#include "arim_query.h"
#include "arim_ping.h"
#include "arim_arq.h"
#include "arim_arq_files.h"
#include "arim_arq_msg.h"
#include "arim_arq_auth.h"
#include "cmdthread.h"
#include "datathread.h"
#include "ini.h"
#include "ui.h"
#include "ui_dialog.h"
#include "ui_files.h"
#include "ui_msg.h"
#include "ui_themes.h"
#include "ui_recents.h"
#include "ui_ping_hist.h"
#include "ui_conn_hist.h"
#include "ui_file_hist.h"
#include "ui_heard_list.h"
#include "ui_tnc_data_win.h"
#include "util.h"
#include "auth.h"
#include "bufq.h"
#include "cmdproc.h"
#include "tnc_attach.h"

#define MSG_SEND_FAIL_PROMPT_SAVE   1

int cmdproc_cmd(const char *cmd)
{
    static char prevbuf[MAX_CMD_SIZE];
    int state, result1, result2, numch, zoption = 0;
    char *t, *fn, *destdir, buffer[MAX_CMD_SIZE], sendcr[TNC_ARQ_SENDCR_SIZE];
    char msgbuffer[MAX_UNCOMP_DATA_SIZE], status[MAX_STATUS_BAR_SIZE];
    char call1[TNC_MYCALL_SIZE], call2[TNC_MYCALL_SIZE];
    const char *p;

    state = arim_get_state();
    switch (state) {
    case ST_ARQ_FILE_RCV_WAIT_OK:
    case ST_ARQ_FILE_RCV_WAIT:
    case ST_ARQ_FILE_RCV:
        /* busy with file download */
        ui_print_status("ARIM Busy: ARQ file download in progress", 1);
        return 0;
    case ST_ARQ_FILE_SEND_WAIT_OK:
    case ST_ARQ_FILE_SEND_WAIT:
    case ST_ARQ_FILE_SEND:
        /* busy with file upload */
        ui_print_status("ARIM Busy: ARQ file upload in progress", 1);
        return 0;
    case ST_ARQ_FLIST_RCV_WAIT:
    case ST_ARQ_FLIST_RCV:
        /* busy with file listing download */
        ui_print_status("ARIM Busy: ARQ file listing download in progress", 1);
        return 0;
    case ST_ARQ_FLIST_SEND_WAIT:
    case ST_ARQ_FLIST_SEND:
        /* busy with file listing upload */
        ui_print_status("ARIM Busy: ARQ file listing upload in progress", 1);
        return 0;
    case ST_ARQ_MSG_RCV:
        /* busy with message receive */
        ui_print_status("ARIM Busy: ARQ message download in progress", 1);
        return 0;
    case ST_ARQ_MSG_SEND_WAIT:
    case ST_ARQ_MSG_SEND:
        /* busy with message upload */
        ui_print_status("ARIM Busy: ARQ message upload in progress", 1);
        return 0;
    case ST_ARQ_IN_CONNECT_WAIT:
    case ST_ARQ_OUT_CONNECT_WAIT:
        /* busy with connect attempt */
        ui_print_status("ARIM Busy: ARQ connect in progress", 1);
        return 0;
    case ST_ARQ_AUTH_RCV_A2_WAIT:
    case ST_ARQ_AUTH_RCV_A3_WAIT:
    case ST_ARQ_AUTH_RCV_A4_WAIT:
    case ST_ARQ_AUTH_SEND_A1:
    case ST_ARQ_AUTH_SEND_A2:
    case ST_ARQ_AUTH_SEND_A3:
        /* busy with authentication exchange */
        ui_print_status("ARIM Busy: ARQ mutual authentication in progress", 1);
        return 0;
    case ST_ARQ_CONNECTED:
        if (!strncasecmp(cmd, "/DIS", 4)) {
            result1 = ui_show_dialog(
               "\tAre you sure\n\tyou want to disconnect?\n \n\t[Y]es   [N]o", "yYnN");
            if (result1 == 'y' || result1 == 'Y')
                arim_arq_send_disconn_req();
        } else {
            if (!strncasecmp(cmd, "/FGET", 5)) {
                arim_arq_cache_cmd(cmd);
                /* check for -z option */
                snprintf(msgbuffer, sizeof(msgbuffer), "%s", cmd + 5);
                fn = msgbuffer;
                while (*fn && *fn == ' ')
                    ++fn;
                if (*fn && (fn == strstr(fn, "-z"))) {
                    zoption = 1;
                    fn += 2;
                } else {
                    /* -z option not found, back up to start */
                    fn = msgbuffer;
                }
                /* check for destination dir path */
                destdir = fn;
                while (*destdir && *destdir != '>')
                    ++destdir;
                if (*destdir == '>')
                    *destdir++ = '\0';
                else
                    destdir = NULL;
                arim_arq_files_on_client_fget(cmd, fn, destdir, zoption);
                return 1;
            } else if (!strncasecmp(cmd, "/FPUT", 5)) {
                arim_arq_cache_cmd(cmd);
                /* check for -z option */
                snprintf(msgbuffer, sizeof(msgbuffer), "%s", cmd + 5);
                fn = msgbuffer;
                while (*fn && *fn == ' ')
                    ++fn;
                if (*fn && (fn == strstr(fn, "-z"))) {
                    zoption = 1;
                    fn += 2;
                } else {
                    /* -z option not found, back up to start */
                    fn = msgbuffer;
                }
                /* check for destination dir path */
                destdir = fn;
                while (*destdir && *destdir != '>')
                    ++destdir;
                if (*destdir == '>')
                    *destdir++ = '\0';
                else
                    destdir = NULL;
                arim_arq_files_on_client_fput(fn, destdir, zoption);
                return 1;
            } else if (!strncasecmp(cmd, "/MGET", 5)) {
                arim_arq_cache_cmd(cmd);
                /* check for -z option */
                snprintf(msgbuffer, sizeof(msgbuffer), "%s", cmd + 5);
                t = msgbuffer;
                while (*t && *t == ' ')
                    ++t;
                if (*t && (t == strstr(t, "-z"))) {
                    zoption = 1;
                    t += 2;
                } else {
                    /* -z option not found, back up to start */
                    t = msgbuffer;
                }
                arim_arq_msg_on_client_mget(cmd, t, zoption);
                return 1;
            } else if (!strncasecmp(cmd, "/FLGET", 6)) {
                arim_arq_cache_cmd(cmd);
                /* check for -z option */
                snprintf(buffer, sizeof(buffer), "%s", cmd + 6);
                t = buffer;
                while (*t && (*t == ' ' || *t == '/'))
                    ++t;
                if (*t && (t == strstr(t, "-z"))) {
                    zoption = 1;
                    t += 2;
                    while (*t && (*t == ' ' || *t == '/'))
                        ++t;
                }
                /* check for dir path */
                destdir = (*t) ? t : NULL;
                arim_arq_files_on_client_flget(cmd, destdir, zoption);
                return 1;
            } else if (!strncasecmp(cmd, "/FLIST", 6)) {
                arim_arq_cache_cmd(cmd);
                arim_arq_files_on_client_flist(cmd);
                return 1;
            } else if (!strncasecmp(cmd, "/FILE", 5)) {
                arim_arq_cache_cmd(cmd);
                arim_arq_files_on_client_file(cmd);
                return 1;
            } else if (!strncasecmp(cmd, "/MLIST", 6)) {
                arim_arq_cache_cmd(cmd);
                arim_arq_msg_on_client_mlist(cmd);
                return 1;
            } else if (!strncasecmp(cmd, "/AUTH", 5)) {
                arim_arq_cache_cmd(cmd);
                arim_arq_auth_on_client_challenge(cmd);
                return 1;
            } else if (!strncasecmp(cmd, "/SM", 3)) {
                arim_arq_cache_cmd(cmd);
                /* check for -z option */
                p = cmd + 3;
                while (*p && *p == ' ')
                    ++p;
                if (*p && (p == strstr(p, "-z"))) {
                    zoption = 1;
                    p += 2;
                } else {
                    /* -z option not found, back up to start */
                    p = cmd + 3;
                }
                snprintf(buffer, sizeof(buffer), "%s", p);
                /* check for message text on command line */
                t = strtok(buffer, "\0");
                if (!t) {
                    /* no message text, open composer view */
                    arim_copy_remote_call(buffer, sizeof(buffer));
                    if (ui_create_msg(msgbuffer, sizeof(msgbuffer), buffer))
                        arim_arq_msg_on_send_cmd(msgbuffer, zoption);
                } else {
                    snprintf(msgbuffer, sizeof(msgbuffer), "%s", t);
                    arim_arq_msg_on_send_cmd(msgbuffer, zoption);
                }
                return 1;
            }
            arim_copy_arq_sendcr(sendcr, sizeof(sendcr));
            if (!strncasecmp(sendcr, "TRUE", 4))
                snprintf(buffer, sizeof(buffer), "%s\r\n", cmd);
            else
                snprintf(buffer, sizeof(buffer), "%s\n", cmd);
            bufq_queue_data_out(buffer);
        }
        return 1;
    default:
        if (!strlen(cmd))
            return 0;
        break;
    }
    if (!strncmp(cmd, "!!", 2)) {
        memcpy(buffer, prevbuf, sizeof(buffer));
    } else {
        snprintf(buffer, sizeof(buffer), "%s", cmd);
        memcpy(prevbuf, buffer, sizeof(prevbuf));
    }
    switch (buffer[0]) {
    case ':':
        if (!arim_is_idle() || !arim_tnc_is_idle()) {
            ui_print_status("TNC Busy: cannot send unproto message", 1);
            break;
        }
        result1 = arim_get_state();
        /* allow sending of text when TNC already busy with previous unproto send */
        if (g_tnc_attached && (result1 == ST_IDLE || result1 == ST_SEND_UN_BUF_WAIT)) {
            bufq_queue_data_out(&buffer[1]);
            if (!arim_get_buffer_cnt()) {
                /* prime buffer count because update from TNC not immediate */
                pthread_mutex_lock(&mutex_tnc_set);
                snprintf(g_tnc_settings[g_cur_tnc].buffer,
                    sizeof(g_tnc_settings[g_cur_tnc].buffer), "%zu", strlen(&buffer[1]));
                pthread_mutex_unlock(&mutex_tnc_set);
            }
            arim_on_event(EV_SEND_UNPROTO, 0);
        } else {
            ui_print_status("TNC Busy: cannot send unproto message", 1);
        }
        break;
    case '!':
        if (g_tnc_attached) {
            bufq_queue_cmd_out(&buffer[1]);
        }
        break;
    case '.':
        t = strtok(buffer, " \t\n");
        if (t && !strncasecmp(t, ".crc", 4)) {
            t = strtok(NULL, "\n\0");
            if (t) {
                result1 = ccitt_crc16((unsigned char *)t, strlen(t));
                snprintf(status, sizeof(status), "crc: %04X", result1);
                ui_print_status(status, 1);
            }
        }
        if (t && !strncasecmp(t, ".b64", 4)) {
            t = strtok(NULL, "\n\0");
            if (t) {
                if (auth_base64_encode((unsigned char *)t, strlen(t), msgbuffer, sizeof(msgbuffer))) {
                    numch = snprintf(status, sizeof(status), "b64: %s", msgbuffer);
                    ui_print_status(status, 1);
                } else {
                    ui_print_status("Failed to encode input", 1);
                }
            }
        }
        break;
    default:
        t = strtok(buffer, " \t");
        if (!t)
            break;
        if (strlen(t) == 2 && !strncasecmp(t, "li", 2)) {
            ui_print_status("Listing messages in inbox", 1);
            ui_list_msg(MBOX_INBOX_FNAME, MBOX_TYPE_IN);
        } else if (!strncasecmp(t, "lo", 2)) {
            ui_print_status("Listing messages in outbox", 1);
            ui_list_msg(MBOX_OUTBOX_FNAME, MBOX_TYPE_OUT);
        } else if (!strncasecmp(t, "ls", 2)) {
            ui_print_status("Listing sent messages", 1);
            ui_list_msg(MBOX_SENTBOX_FNAME, MBOX_TYPE_SENT);
        } else if (!strncasecmp(t, "lf", 2)) {
            ui_print_status("Listing shared files directory", 1);
            ui_list_shared_files();
        ////////////////////////////
        // hamlib - 20230115
        } else if (!strncasecmp(t, "qsy", 3)) {
            // parse out tnc #
            t = strtok(NULL, " \t");
            if (!t) {
               break;
            }
            // find the rig
            int tnc_id = atoi(t);
            RIG *rig = g_tnc_settings[tnc_id].hamlib_rig;

            // parse frequency
            t = strtok(NULL, " \t");
            double freq;
            sscanf(t, "%lf", &freq);
            int rc = rig_set_freq(rig, RIG_VFO_A, freq);
            if (rc != RIG_OK) {
               // squawk an error
               ui_print_status("setting freq failed", 1);
            }
            ui_print_status("Changed frequency!", 1);
        ////////////////////////////
        } else if (!strncasecmp(t, "sm", 2)) {
            t = strtok(NULL, " \t");
            if (t && !strcmp(t, "-z")) {
                ui_print_status("Send msg: -z option not supported in FEC mode", 1);
                break;
            }
            if (!t || (!ini_validate_mycall(t) && !ini_validate_netcall(t))) {
                ui_print_status("Send msg: cannot send, invalid call sign", 1);
                break;
            }
            snprintf(call1, sizeof(call1), "%s", t);
            /* now get everything up to end of line */
            t = strtok(NULL, "\0");
            if (!t && ui_create_msg(msgbuffer, sizeof(msgbuffer), call1)) {
                if (g_tnc_attached) {
                    if (arim_send_msg(msgbuffer, call1)) {
                        ui_print_status("ARIM Busy: sending message", 1);
                    } else {
#ifdef MSG_SEND_FAIL_PROMPT_SAVE
                        result1 = ui_show_dialog("\tCannot send message, TNC is busy!\n"
                                                 "\tDo you want to save\n"
                                                 "\tthe message to your Outbox?\n \n\t[Y]es   [N]o", "yYnN");
                        if (result1 == 'y' || result1 == 'Y') {
                            ui_print_status("Saving message to Outbox...", 1);
                            arim_store_out(msgbuffer, call1);
                        }
#else
                        arim_store_out(msgbuffer, call1);
                        ui_print_status("Send msg: cannot send, TNC busy; saved to Outbox", 1);
#endif
                    }
                } else {
#ifdef MSG_SEND_FAIL_PROMPT_SAVE
                    result1 = ui_show_dialog("\tCannot send message, no TNC attached!\n"
                                             "\tDo you want to save\n"
                                             "\tthe message to your Outbox?\n \n\t[Y]es   [N]o", "yYnN");
                    if (result1 == 'y' || result1 == 'Y') {
                        ui_print_status("Saving message to Outbox...", 1);
                        arim_store_out(msgbuffer, call1);
                    }
#else
                    arim_store_out(msgbuffer, call1);
                    ui_print_status("Send msg: cannot send, no TNC attached; saved to Outbox", 1);
#endif
                }
            } else if (t) {
                snprintf(msgbuffer, sizeof(msgbuffer), "%s", t);
                if (g_tnc_attached) {
                    if (arim_send_msg(msgbuffer, call1)) {
                        ui_print_status("ARIM Busy: sending message", 1);
                    } else {
#ifdef MSG_SEND_FAIL_PROMPT_SAVE
                        result1 = ui_show_dialog("\tCannot send message, TNC is busy!\n"
                                                 "\tDo you want to save\n"
                                                 "\tthe message to your Outbox?\n \n\t[Y]es   [N]o", "yYnN");
                        if (result1 == 'y' || result1 == 'Y') {
                            ui_print_status("Saving message to Outbox...", 1);
                            arim_store_out(msgbuffer, call1);
                        }
#else
                        arim_store_out(msgbuffer, call1);
                        ui_print_status("Send msg: cannot send, TNC busy; saved to Outbox", 1);
#endif
                    }
                } else {
#ifdef MSG_SEND_FAIL_PROMPT_SAVE
                    result1 = ui_show_dialog("\tCannot send message, no TNC attached!\n"
                                             "\tDo you want to save\n"
                                             "\tthe message to your Outbox?\n \n\t[Y]es   [N]o", "yYnN");
                    if (result1 == 'y' || result1 == 'Y') {
                        ui_print_status("Saving message to Outbox...", 1);
                        arim_store_out(msgbuffer, call1);
                    }
#else
                    arim_store_out(msgbuffer, call1);
                    ui_print_status("Send msg: cannot send, no TNC attached; saved to Outbox", 1);
#endif
                }
            }
        } else if (!strncasecmp(t, "sq", 2)) {
            if (!g_tnc_attached) {
                ui_print_status("Send query: cannot send, no TNC attached", 1);
                break;
            }
            t = strtok(NULL, " \t");
            if (!t || !ini_validate_mycall(t)) {
                ui_print_status("Send query: invalid call sign", 1);
                break;
            }
            snprintf(call1, sizeof(call1), "%s", t);
            /* now get everything up to end of line */
            t = strtok(NULL, "\0");
            if (!t)
                break;
            if (arim_send_query(t, call1))
                ui_print_status("ARIM Busy: sending query", 1);
            else
                ui_print_status("Send query: cannot send, TNC busy", 1);
        } else if (!strncasecmp(t, "ping", 2)) {
            if (!g_tnc_attached) {
                ui_print_status("Send ping: cannot send, no TNC attached", 1);
                break;
            }
            t = strtok(NULL, " \t");
            if (!t || !ini_validate_mycall(t)) {
                ui_print_status("Send ping: invalid call sign", 1);
                break;
            }
            snprintf(call1, sizeof(call1), "%s", t);
            /* now get everything up to end of line */
            t = strtok(NULL, "\0");
            if (!t || (result1 = atoi(t)) < 2 || result1 > 15) {
                ui_print_status("Send ping: invalid repeat count", 1);
                break;
            }
            if (!arim_is_idle() || !arim_send_ping(t, call1, 1)) {
                ui_print_status("Send ping: cannot send, TNC busy", 1);
            }
        } else if (!strncasecmp(t, "conn", 2)) {
            if (!g_tnc_attached) {
                ui_print_status("ARQ Connect: cannot connect, no TNC attached", 1);
                break;
            }
            /* parse call sign */
            t = strtok(NULL, " \t");
            if (!t || !ini_validate_mycall(t)) {
                ui_print_status("ARQ Connect: invalid call sign", 1);
                break;
            }
            snprintf(call1, sizeof(call1), "%s", t);
            /* parse number of repeats */
            t = strtok(NULL, " \t");
            if (!t || (result1 = atoi(t)) < 3 || result1 > 15) {
                ui_print_status("ARQ Connect: invalid repeat count", 1);
                break;
            }
            /* now get optional ARQBW parameter */
            t = strtok(NULL, " \0");
            if (t && !ini_validate_arq_bw(t) && strncasecmp(t, "any", 3)) {
                ui_print_status("ARQ Connect: invalid ARQBW parameter", 1);
                break;
            }
            if (!arim_arq_send_conn_req(result1, call1, t))
                ui_print_status("ARQ Connect: cannot send connection request, TNC busy", 1);
            else
                ui_print_status("ARIM Busy: sending connection request", 1);
        } else if (!strncasecmp(t, "cm", 2)) {
            t = strtok(NULL, " \t");
            if (!t || (!ini_validate_mycall(t) && !ini_validate_netcall(t))) {
                ui_print_status("Compose msg: cannot open, invalid call sign", 1);
                break;
            }
            if (ui_create_msg(msgbuffer, sizeof(msgbuffer), t)) {
                arim_store_out(msgbuffer, t);
                ui_print_status("Compose msg: message saved to Outbox", 1);
            }
        } else if (!strncasecmp(t, "rr", 2) && show_recents) {
            t = strtok(NULL, " \t");
            if (!t || (result1 = atoi(t)) < 1) {
                ui_print_status("Read recent: invalid msg number", 1);
                break;
            }
            if (ui_get_recent(result1 - 1, buffer, sizeof(buffer))) {
                ui_read_msg(MBOX_INBOX_FNAME, buffer, result1, 1);
                ui_set_recent_flag(buffer, 'R');
                ui_refresh_recents();
            } else {
                ui_print_status("Read recent: cannot read message", 1);
            }
        } else if (!strncasecmp(t, "passwd", 4)) {
            t = strtok(NULL, " \t");
            if (!t || !ini_validate_mycall(t)) {
                ui_print_status("Passwd: invalid remote station call sign", 1);
                break;
            }
            snprintf(call1, sizeof(call1), "%s", t);
            t = strtok(NULL, " \t");
            if (!t || !ini_validate_mycall(t)) {
                ui_print_status("Passwd: invalid local station call sign", 1);
                break;
            }
            snprintf(call2, sizeof(call2), "%s", t);
            /* now get everything up to end of line */
            t = strtok(NULL, "\0");
            if (!t) {
                ui_print_status("Passwd: no password found", 1);
                break;
            } else {
                snprintf(buffer, sizeof(buffer), "%s", t);
                snprintf(msgbuffer, sizeof(msgbuffer),
                         "\tChange/add password\n \n"
                         "   For client station: %s\n"
                         "on server station TNC: %s\n"
                         "         New password: %s\n"
                         " \n\tAre you sure?\n \n\t[Y]es   [N]o",
                         call1, call2, buffer);
                result1 = ui_show_dialog(msgbuffer, "yYnN");
                if (result1 == 'y' || result1 == 'Y') {
                    if (auth_store_passwd(call1, call2, buffer)) {
                        snprintf(buffer, sizeof(buffer),
                            "Stored password for client stn: %s and server stn: %s",
                                call1, call2);
                        ui_print_status(buffer, 1);
                    } else {
                        ui_print_status("Passwd: failed to store password in file", 1);
                        break;
                    }
                }
            }
        } else if (!strncasecmp(t, "delpass", 4)) {
            t = strtok(NULL, " \t");
            if (!t || !ini_validate_mycall(t)) {
                ui_print_status("Delpass: invalid remote station call sign", 1);
                break;
            }
            snprintf(call1, sizeof(call1), "%s", t);
            t = strtok(NULL, " \t");
            if (!t || !ini_validate_mycall(t)) {
                ui_print_status("Delpass: invalid local station call sign", 1);
                break;
            }
            snprintf(call2, sizeof(call2), "%s", t);
            snprintf(msgbuffer, sizeof(msgbuffer),
                     "\tDelete password\n \n"
                         "   For client station: %s\n"
                         "on server station TNC: %s\n"
                     " \n\tAre you sure?\n \n\t[Y]es   [N]o",
                     call1, call2);
            result1 = ui_show_dialog(msgbuffer, "yYnN");
            if (result1 == 'y' || result1 == 'Y') {
                if (auth_delete_passwd(call1, call2)) {
                    snprintf(buffer, sizeof(buffer),
                        "Deleted password for client stn: %s and server stn: %s",
                            call1, call2);
                    ui_print_status(buffer, 1);
                } else {
                    ui_print_status("Delpass: failed to delete password in file", 1);
                    break;
                }
            }
        } else if (!strncasecmp(t, "att", 3) && !g_tnc_attached) {
            t = strtok(NULL, " \t");
            if (!t)
                break;
            result1 = atoi(t);
            if (result1 > 0 && result1 <= g_num_tnc) {
                result1--;
                tnc_attach(result1);
            } else {
                ui_print_status("Invalid TNC number", 1);
            }
        } else if (!strncasecmp(t, "det", 3) && g_tnc_attached) {
            tnc_detach();
        } else if (!strncasecmp(t, "listen", 4)) {
            if (g_tnc_attached) {
                t = strtok(NULL, " \t");
                if (!t)
                    break;
                result1 = !strncasecmp(t, "TRUE", 1);
                result2 = !strncasecmp(t, "FALSE", 1);
                pthread_mutex_lock(&mutex_tnc_set);
                if (result1) {
                    snprintf(g_tnc_settings[g_cur_tnc].listen, sizeof(g_tnc_settings[g_cur_tnc].listen), "%s", "TRUE");
                } else if (result2) {
                    snprintf(g_tnc_settings[g_cur_tnc].listen, sizeof(g_tnc_settings[g_cur_tnc].listen), "%s", "FALSE");
                }
                pthread_mutex_unlock(&mutex_tnc_set);
                if (result1) {
                    bufq_queue_cmd_out("LISTEN TRUE");
                    ui_print_status("Listening for ARQ connect requests and pings enabled", 1);
                } else if (result2) {
                    bufq_queue_cmd_out("LISTEN FALSE");
                    ui_print_status("Listening for ARQ connect requests and pings disabled", 1);
                } else {
                    ui_print_status("Invalid ARQ listen value, must be T(rue) or F(alse)", 1);
                }
            } else {
                ui_print_status("Cannot set ARQ listen: no TNC attached", 1);
            }
        } else if (!strncasecmp(t, "arqset", 4)) {
            if (g_tnc_attached) {
                pthread_mutex_lock(&mutex_tnc_set);
                snprintf(msgbuffer, sizeof(msgbuffer),
                    "\tARQ SESSION SETTINGS\n \n"
                    "\tarq-bandwidth: %.4s\n"
                    "\tarq-negotiate-bw: %.5s\n"
                    "\tarq-timeout: %.3s\n"
                    "\tarq-sendcr: %.5s\n \n\t[O]k",
                        g_tnc_settings[g_cur_tnc].arq_bandwidth,
                        g_tnc_settings[g_cur_tnc].arq_negotiate_bw,
                        g_tnc_settings[g_cur_tnc].arq_timeout,
                        g_tnc_settings[g_cur_tnc].arq_sendcr);
                pthread_mutex_unlock(&mutex_tnc_set);
                ui_show_dialog(msgbuffer, " oO\n");
            } else {
                ui_print_status("Cannot show ARQ settings: no TNC attached", 1);
            }
        } else if (!strncasecmp(t, "arqto", 4)) {
            if (g_tnc_attached) {
                t = strtok(NULL, " \t");
                if (!t)
                    break;
                result1 = atoi(t);
                if (result1 >= MIN_TNC_ARQ_TO && result1 <= MAX_TNC_ARQ_TO) {
                    pthread_mutex_lock(&mutex_tnc_set);
                    snprintf(g_tnc_settings[g_cur_tnc].arq_timeout,
                                sizeof(g_tnc_settings[g_cur_tnc].arq_timeout), "%d", result1);
                    pthread_mutex_unlock(&mutex_tnc_set);
                    snprintf(status, sizeof(status), "ARQTIMEOUT %d", result1);
                    bufq_queue_cmd_out(status);
                    snprintf(status, sizeof(status), "ARQ connection timeout: %d", result1);
                    ui_print_status(status, 1);
                } else {
                    ui_print_status("Invalid ARQ timeout, must be between 30 and 600 seconds", 1);
                }
            } else {
                ui_print_status("Cannot show ARQ settings: no TNC attached", 1);
            }
        } else if (!strncasecmp(t, "arqbw", 4)) {
            if (g_tnc_attached) {
                t = strtok(NULL, " \t");
                if (!t)
                    break;
                snprintf(buffer, sizeof(buffer), "%s", t);
                /* force bandwidth specifier to uppercase */
                result1 = strlen(buffer);
                for (result2 = 0; result2 < result1; result2++)
                    buffer[result2] = toupper(buffer[result2]);
                if (ini_validate_arq_bw(buffer)) {
                    pthread_mutex_lock(&mutex_tnc_set);
                    numch = snprintf(g_tnc_settings[g_cur_tnc].arq_bandwidth,
                                     sizeof(g_tnc_settings[g_cur_tnc].arq_bandwidth), "%s", buffer);
                    pthread_mutex_unlock(&mutex_tnc_set);
                    numch = snprintf(status, sizeof(status), "ARQBW %s", buffer);
                    bufq_queue_cmd_out(status);
                    numch = snprintf(status, sizeof(status), "ARQ connection bandwidth: %s", buffer);
                    ui_print_status(status, 1);
                } else {
                    ui_print_status("Invalid ARQ bandwidth value", 1);
                }
            } else {
                ui_print_status("Cannot show ARQ settings: no TNC attached", 1);
            }
        } else if (!strncasecmp(t, "arqnegbw", 4)) {
            if (g_tnc_attached) {
                t = strtok(NULL, " \t");
                if (!t)
                    break;
                result1 = !strncasecmp(t, "TRUE", 1);
                result2 = !strncasecmp(t, "FALSE", 1);
                pthread_mutex_lock(&mutex_tnc_set);
                if (result1) {
                    snprintf(g_tnc_settings[g_cur_tnc].arq_negotiate_bw, sizeof(g_tnc_settings[g_cur_tnc].arq_negotiate_bw), "%s", "TRUE");
                } else if (result2) {
                    snprintf(g_tnc_settings[g_cur_tnc].arq_negotiate_bw, sizeof(g_tnc_settings[g_cur_tnc].arq_negotiate_bw), "%s", "FALSE");
                }
                pthread_mutex_unlock(&mutex_tnc_set);
                if (result1) {
                    bufq_queue_cmd_out("NEGOTIATEBW TRUE");
                    ui_print_status("ARQ bandwidth negotiation enabled", 1);
                } else if (result2) {
                    bufq_queue_cmd_out("NEGOTIATEBW  FALSE");
                    ui_print_status("ARQ bandwidth negotiation disabled", 1);
                } else {
                    ui_print_status("Invalid ARQ negotiatebw value, must be T(rue) or F(alse)", 1);
                }
            } else {
                ui_print_status("Cannot set ARQ negotiatebw: no TNC attached", 1);
            }
        } else if (!strncasecmp(t, "srset", 4)) {
            snprintf(msgbuffer, sizeof(msgbuffer),
                "\tMESSAGE SEND REPEAT SETTINGS\n \n"
                "\tsend-repeats: %.1s\n"
                "\tack-timeout: %.3s\n"
                "\tfecmode-downshift: %.5s\n \n\t[O]k",
                    g_arim_settings.send_repeats, g_arim_settings.ack_timeout,
                    g_arim_settings.fecmode_downshift);
            ui_show_dialog(msgbuffer, " oO\n");
        } else if (!strncasecmp(t, "ppset", 4)) {
            snprintf(msgbuffer, sizeof(msgbuffer),
                "\tPILOT PING SETTINGS\n \n"
                "\tpilot-ping: %.1s\n"
                "\tpilot-ping-thr: %.3s\n \n\t[O]k",
                   g_arim_settings.pilot_ping, g_arim_settings.pilot_ping_thr);
            ui_show_dialog(msgbuffer, " oO\n");
        } else if (!strncasecmp(t, "tncset", 4)) {
            if (g_tnc_attached) {
                pthread_mutex_lock(&mutex_tnc_set);
                if (!strncasecmp(g_tnc_settings[g_cur_tnc].interface, "tcp", 3)) {
                    snprintf(msgbuffer, sizeof(msgbuffer),
                    "\tARDOP TNC SETTINGS\n \n"
                    "   ip addr: %-10.30s       port: %.5s\n"
                    "    mycall: %-10.10s     gridsq: %.10s\n"
                    "     fecid: %-10.8s fecrepeats: %.5s\n"
                    "   squelch: %-10.2s    busydet: %.2s\n"
                    "    leader: %-10.4s    trailer: %.4s\n"
                    "    listen: %-10.5s  enpingack: %.5s\n"
                    "   busydet: %-10.5s      arqbw: %.10s\n"
                    " netcall 1: %-10.10s  netcall 2: %.10s\n"
                    " netcall 3: %-10.10s  netcall 4: %.10s\n"
                    " netcall 5: %-10.10s  netcall 6: %.10s\n"
                    " netcall 7: %-10.10s  netcall 8: %.10s\n"
                    " hamlib_model: %d\n"
                    "   version: %.25s\n \n\t[O]k",
                    g_tnc_settings[g_cur_tnc].ipaddr, g_tnc_settings[g_cur_tnc].port,
                    g_tnc_settings[g_cur_tnc].mycall, g_tnc_settings[g_cur_tnc].gridsq,
                    g_tnc_settings[g_cur_tnc].fecid, g_tnc_settings[g_cur_tnc].fecrepeats,
                    g_tnc_settings[g_cur_tnc].squelch, g_tnc_settings[g_cur_tnc].busydet,
                    g_tnc_settings[g_cur_tnc].leader, g_tnc_settings[g_cur_tnc].trailer,
                    g_tnc_settings[g_cur_tnc].listen, g_tnc_settings[g_cur_tnc].en_pingack,
                    g_tnc_settings[g_cur_tnc].busydet, g_tnc_settings[g_cur_tnc].arq_bandwidth,
                    g_tnc_settings[g_cur_tnc].netcall[0], g_tnc_settings[g_cur_tnc].netcall[1],
                    g_tnc_settings[g_cur_tnc].netcall[2], g_tnc_settings[g_cur_tnc].netcall[3],
                    g_tnc_settings[g_cur_tnc].netcall[4], g_tnc_settings[g_cur_tnc].netcall[5],
                    g_tnc_settings[g_cur_tnc].netcall[6], g_tnc_settings[g_cur_tnc].netcall[7],
                    g_tnc_settings[g_cur_tnc].hamlib_model,
                    g_tnc_settings[g_cur_tnc].version);
                } else {
                    snprintf(msgbuffer, sizeof(msgbuffer),
                    "\tARDOP TNC SETTINGS\n \n"
                    "serial dev: %-12.30s  baud rate: %.6s\n"
                    "    mycall: %-12.10s     gridsq: %.10s\n"
                    "     fecid: %-12.8s fecrepeats: %.5s\n"
                    "   squelch: %-12.2s    busydet: %.2s\n"
                    "    leader: %-12.4s    trailer: %.4s\n"
                    "    listen: %-12.5s  enpingack: %.5s\n"
                    "   busydet: %-12.5s      arqbw: %.10s\n"
                    " netcall 1: %-12.10s  netcall 2: %.10s\n"
                    " netcall 3: %-12.10s  netcall 4: %.10s\n"
                    " netcall 5: %-12.10s  netcall 6: %.10s\n"
                    " netcall 7: %-12.10s  netcall 8: %.10s\n"
                    " hamlib_model: %d\n"
                    "   version: %.25s\n \n\t[O]k",
                    g_tnc_settings[g_cur_tnc].serial_port, g_tnc_settings[g_cur_tnc].serial_baudrate,
                    g_tnc_settings[g_cur_tnc].mycall, g_tnc_settings[g_cur_tnc].gridsq,
                    g_tnc_settings[g_cur_tnc].fecid, g_tnc_settings[g_cur_tnc].fecrepeats,
                    g_tnc_settings[g_cur_tnc].squelch, g_tnc_settings[g_cur_tnc].busydet,
                    g_tnc_settings[g_cur_tnc].leader, g_tnc_settings[g_cur_tnc].trailer,
                    g_tnc_settings[g_cur_tnc].listen, g_tnc_settings[g_cur_tnc].en_pingack,
                    g_tnc_settings[g_cur_tnc].busydet, g_tnc_settings[g_cur_tnc].arq_bandwidth,
                    g_tnc_settings[g_cur_tnc].netcall[0], g_tnc_settings[g_cur_tnc].netcall[1],
                    g_tnc_settings[g_cur_tnc].netcall[2], g_tnc_settings[g_cur_tnc].netcall[3],
                    g_tnc_settings[g_cur_tnc].netcall[4], g_tnc_settings[g_cur_tnc].netcall[5],
                    g_tnc_settings[g_cur_tnc].netcall[6], g_tnc_settings[g_cur_tnc].netcall[7],
                    g_tnc_settings[g_cur_tnc].hamlib_model,
                    g_tnc_settings[g_cur_tnc].version);
                }
                pthread_mutex_unlock(&mutex_tnc_set);
                ui_show_dialog(msgbuffer, " oO\n");
            } else {
                ui_print_status("Cannot show TNC settings: no TNC attached", 1);
            }
        } else if (!strncasecmp(t, "ackto", 4)) {
            t = strtok(NULL, " \t");
            if (!t)
                break;
            result1 = atoi(t);
            if (result1 >= MIN_ARIM_ACK_TIMEOUT && result1 <= MAX_ARIM_ACK_TIMEOUT) {
                snprintf(g_arim_settings.ack_timeout, sizeof(g_arim_settings.ack_timeout), "%d", result1);
                snprintf(status, sizeof(status), "ARIM message ACK timeout: %d", result1);
                ui_print_status(status, 1);
            } else {
                ui_print_status("Invalid ACK timeout, must be between 10 and 999 seconds", 1);
            }
        } else if (!strncasecmp(t, "srpts", 4)) {
            t = strtok(NULL, " \t");
            if (!t)
                break;
            result1 = atoi(t);
            if (result1 >= 0 && result1 <= MAX_ARIM_SEND_REPEATS) {
                snprintf(g_arim_settings.send_repeats, sizeof(g_arim_settings.send_repeats), "%d", result1);
                snprintf(status, sizeof(status), "ARIM message send repeats: %d", result1);
                ui_print_status(status, 1);
            } else {
                ui_print_status("Invalid send repeats value, must be between 0 and 5", 1);
            }
        } else if (!strncasecmp(t, "pping", 4)) {
            t = strtok(NULL, " \t");
            if (!t)
                break;
            result1 = atoi(t);
            if (result1 == 0 || (result1 >= MIN_ARIM_PILOT_PING && result1 <= MAX_ARIM_PILOT_PING)) {
                snprintf(g_arim_settings.pilot_ping, sizeof(g_arim_settings.pilot_ping), "%d", result1);
                snprintf(status, sizeof(status), "ARIM message pilot ping: %d", result1);
                ui_print_status(status, 1);
            } else {
                ui_print_status("Invalid pilot ping value, must be 0 (off) or between 2 and 5", 1);
            }
        } else if (!strncasecmp(t, "ppthr", 4)) {
            t = strtok(NULL, " \t");
            if (!t)
                break;
            result1 = atoi(t);
            if (result1 >= MIN_ARIM_PILOT_PING_THR && result1 <= MAX_ARIM_PILOT_PING_THR) {
                snprintf(g_arim_settings.pilot_ping_thr, sizeof(g_arim_settings.pilot_ping_thr), "%d", result1);
                snprintf(status, sizeof(status), "ARIM pilot ping threshold: %d", result1);
                ui_print_status(status, 1);
            } else {
                ui_print_status("Invalid pilot ping threshold, must be between 50 and 100", 1);
            }
        } else if (!strncasecmp(t, "enpingack", 4)) {
            if (g_tnc_attached) {
                t = strtok(NULL, " \t");
                if (!t)
                    break;
                result1 = !strncasecmp(t, "TRUE", 1);
                result2 = !strncasecmp(t, "FALSE", 1);
                pthread_mutex_lock(&mutex_tnc_set);
                if (result1) {
                    snprintf(g_tnc_settings[g_cur_tnc].en_pingack, sizeof(g_tnc_settings[g_cur_tnc].en_pingack), "%s", "TRUE");
                } else if (result2) {
                    snprintf(g_tnc_settings[g_cur_tnc].en_pingack, sizeof(g_tnc_settings[g_cur_tnc].en_pingack), "%s", "FALSE");
                }
                pthread_mutex_unlock(&mutex_tnc_set);
                if (result1) {
                    bufq_queue_cmd_out("ENABLEPINGACK TRUE");
                    ui_print_status("Ping ACK enabled", 1);
                } else if (result2) {
                    bufq_queue_cmd_out("ENABLEPINGACK FALSE");
                    ui_print_status("Ping ACK disabled", 1);
                } else {
                    ui_print_status("Invalid enable ping ACK value, must be T(rue) or F(alse)", 1);
                }
            } else {
                ui_print_status("Cannot set enable ping ACK: no TNC attached", 1);
            }
        } else if (!strncasecmp(t, "fecds", 4)) {
            t = strtok(NULL, " \t");
            if (!t)
                break;
            result1 = !strncasecmp(t, "TRUE", 1);
            if (result1) {
                snprintf(g_arim_settings.fecmode_downshift, sizeof(g_arim_settings.fecmode_downshift), "%s", "TRUE");
                ui_print_status("FEC mode downshift enabled", 1);
            } else if (!strncasecmp(t, "FALSE", 1)) {
                snprintf(g_arim_settings.fecmode_downshift, sizeof(g_arim_settings.fecmode_downshift), "%s", "FALSE");
                ui_print_status("FEC mode downshift disabled", 1);
            } else {
                ui_print_status("Invalid FEC mode downshift value, must be T(rue) or F(alse)", 1);
            }
        } else if (!strncasecmp(t, "mycall", 4)) {
            if (!g_tnc_attached) {
                ui_print_status("Cannot change mycall: no TNC attached", 1);
                break;
            }
            t = strtok(NULL, " \t");
            if (!t)
                break;
            snprintf(call1, sizeof(call1), "%s", t);
            result1 = ini_validate_mycall(call1);
            if (result1) {
                snprintf(status, sizeof(status), "MYCALL %s", call1);
                bufq_queue_cmd_out(status);
                snprintf(status, sizeof(status), "mycall changed to: %s", call1);
                ui_print_status(status, 1);
            } else {
                ui_print_status("Invalid call sign, mycall not changed", 1);
            }
        } else if (!strncasecmp(t, "gridsq", 4)) {
            if (!g_tnc_attached) {
                ui_print_status("Cannot change gridsq: no TNC attached", 1);
                break;
            }
            t = strtok(NULL, " \t");
            if (!t)
                break;
            snprintf(buffer, sizeof(buffer), "%s", t);
            result1 = ini_validate_gridsq(buffer);
            if (result1) {
                numch = snprintf(status, sizeof(status), "GRIDSQUARE %s", buffer);
                bufq_queue_cmd_out(status);
                numch = snprintf(status, sizeof(status), "gridsq changed to: %s", buffer);
                ui_print_status(status, 1);
            } else {
                ui_print_status("Invalid grid square, gridsq not changed", 1);
            }
        } else if (!strncasecmp(t, "netcall", 4)) {
            if (!g_tnc_attached) {
                ui_print_status("Cannot change netcalls: no TNC attached", 1);
                break;
            }
            t = strtok(NULL, " \t");
            if (!t)
                break;
            if (!strncasecmp(t, "add", 3)) {
                t = strtok(NULL, " \t");
                if (!t)
                    break;
                snprintf(call1, sizeof(call1), "%s", t);
                result1 = arim_get_netcall_cnt();
                if (result1 < TNC_NETCALL_MAX_CNT) {
                    if (ini_validate_netcall(call1)) {
                        pthread_mutex_lock(&mutex_tnc_set);
                        snprintf(g_tnc_settings[g_cur_tnc].netcall[result1],
                                sizeof(g_tnc_settings[g_cur_tnc].netcall[result1]), "%s", call1);
                        ++g_tnc_settings[g_cur_tnc].netcall_cnt;
                        pthread_mutex_unlock(&mutex_tnc_set);
                        snprintf(status, sizeof(status), "Added netcall: %s", call1);
                        ui_print_status(status, 1);
                    } else {
                        ui_print_status("Invalid net call, not added", 1);
                    }
                } else {
                    ui_print_status("Cannot add netcall: list is full", 1);
                }
            } else if (!strncasecmp(t, "del", 3)) {
                t = strtok(NULL, " \t");
                if (!t)
                    break;
                snprintf(call1, sizeof(call1), "%s", t);
                pthread_mutex_lock(&mutex_tnc_set);
                result1 = g_tnc_settings[g_cur_tnc].netcall_cnt;
                for (result2 = 0; result2 < result1; result2++) {
                    if (!strcasecmp(call1, g_tnc_settings[g_cur_tnc].netcall[result2]))
                        break;
                }
                pthread_mutex_unlock(&mutex_tnc_set);
                if (result2 < result1) {
                    /* found it */
                    pthread_mutex_lock(&mutex_tnc_set);
                    memmove(g_tnc_settings[g_cur_tnc].netcall[result2],
                            g_tnc_settings[g_cur_tnc].netcall[result2 + 1],
                            (TNC_NETCALL_MAX_CNT - result2) * TNC_NETCALL_SIZE);
                    --g_tnc_settings[g_cur_tnc].netcall_cnt;
                    pthread_mutex_unlock(&mutex_tnc_set);
                    snprintf(status, sizeof(status), "Deleted netcall: %s", call1);
                    ui_print_status(status, 1);
                } else {
                    ui_print_status("Cannot delete netcall: call not found", 1);
                    break;
                }
            } else {
                ui_print_status("Sorry, use 'netcall add call' or 'netcall del call'", 1);
            }
        } else if (!strncasecmp(t, "pname", 4)) {
            if (!g_tnc_attached) {
                ui_print_status("Cannot change pname: no TNC attached", 1);
                break;
            }
            t = strtok(NULL, "\t");
            if (!t)
                break;
            snprintf(buffer, sizeof(buffer), "%s", t);
            result1 = ini_validate_name(buffer);
            if (result1) {
                pthread_mutex_lock(&mutex_tnc_set);
                numch = snprintf(g_tnc_settings[g_cur_tnc].name, sizeof(g_tnc_settings[g_cur_tnc].name), "%s", buffer);
                pthread_mutex_unlock(&mutex_tnc_set);
                arim_beacon_set(-1);
                numch = snprintf(status, sizeof(status), "pname changed to: %s", buffer);
                ui_print_status(status, 1);
                ui_set_title_dirty(TITLE_TNC_ATTACHED);
            } else {
                ui_print_status("Invalid name, pname not changed", 1);
            }
        } else if (!strncasecmp(t, "theme", 4)) {
            t = strtok(NULL, "\t");
            if (!t)
                break;
            snprintf(buffer, sizeof(buffer), "%s", t);
            result1 = ui_themes_validate_theme(buffer);
            if (result1 != -1) {
                theme = result1;
                numch = snprintf(status, sizeof(status), "UI theme changed to: %s", buffer);
                ui_print_status(status, 1);
                g_win_changed = 1;
            } else {
                ui_print_status("Invalid theme name, not changed", 1);
            }
        } else if (!strncasecmp(t, "btime", 4) && g_tnc_attached) {
            t = strtok(NULL, " \t");
            if (!t)
                break;
            result1 = atoi(t);
            if (result1 >= 0 && result1 <= MAX_BEACON_TIME) {
                arim_beacon_set(result1);
                if (result1)
                    ui_print_status("Beacon started", 1);
                else
                    ui_print_status("Beacon stopped", 1);
            } else {
                ui_print_status("Invalid beacon time, must be between 0 and 999 minutes", 1);
            }
        } else if (!strncasecmp(t, "btest", 4)) {
            if (g_tnc_attached) {
                if (arim_beacon_send())
                    ui_print_status("ARIM Busy: sending beacon", 1);
                else
                    ui_print_status("Cannot send beacon: TNC busy", 1);
            } else {
                ui_print_status("Cannot send beacon: no TNC attached", 1);
            }
        } else if (!strncasecmp(t, "clrmon", 6)) {
            ui_clear_data_in();
            ui_print_status("Traffic Monitor view cleared", 1);
        } else if (!strncasecmp(t, "clrheard", 8)) {
            ui_clear_calls_heard();
            ui_print_status("Calls Heard list cleared", 1);
        } else if (!strncasecmp(t, "clrping", 7)) {
            ui_clear_ptable();
            ui_print_status("Ping History list cleared", 1);
        } else if (!strncasecmp(t, "clrconn", 7)) {
            ui_clear_ctable();
            ui_print_status("Connection History list cleared", 1);
        } else if (!strncasecmp(t, "clrfile", 7)) {
            ui_clear_ftable();
            ui_print_status("ARQ File History list cleared", 1);
        } else if (!strncasecmp(t, "clrrec", 6)) {
            ui_clear_recents();
            ui_print_status("Recent Messages list cleared", 1);
        }
        break;
    }
    (void)numch; /* suppress 'assigned but not used' warning for dummy var */
    return 1;
}

int cmdproc_query(const char *cmd, char *respbuf, size_t respbufsize)
{
    /* called from data thread, not UI thread */
    char *p, *t, buffer[MAX_CMD_SIZE], remote_call[TNC_MYCALL_SIZE];
    char dpath[MAX_PATH_SIZE];
    size_t i, len, cnt;
    int result;

    if (respbuf)
        respbuf[0] = '\0';
    else
        return CMDPROC_FAIL;
    snprintf(buffer, sizeof(buffer), "%s", cmd);
    t = strtok(buffer, " \t");
    if (!strncasecmp(t, "version", 4)) {
        pthread_mutex_lock(&mutex_tnc_set);
        snprintf(respbuf, respbufsize, "VERSION: ARIM " ARIM_VERSION ", %s\n",
                    g_tnc_settings[g_cur_tnc].version);
        pthread_mutex_unlock(&mutex_tnc_set);
    } else if (!strncasecmp(t, "pname", 4)) {
        pthread_mutex_lock(&mutex_tnc_set);
        snprintf(respbuf, respbufsize, "PNAME: %s\n",
                    g_tnc_settings[g_cur_tnc].name);
        pthread_mutex_unlock(&mutex_tnc_set);
    } else if (!strncasecmp(t, "info", 4)) {
        pthread_mutex_lock(&mutex_tnc_set);
        snprintf(respbuf, respbufsize, "INFO: %s\n",
            g_tnc_settings[g_cur_tnc].info);
        pthread_mutex_unlock(&mutex_tnc_set);
        /* convert "\n" in info text to newline char in response */
        t = respbuf + 1;
        while (*t) {
            if (*t == 'n' && *(t - 1) == '\\') {
                *(t - 1) = ' ';
                *t = '\n';
            }
            ++t;
        }
    } else if (!strncasecmp(t, "gridsq", 4)) {
        pthread_mutex_lock(&mutex_tnc_set);
        snprintf(respbuf, respbufsize, "GRIDSQ: %s\n",
                    g_tnc_settings[g_cur_tnc].gridsq);
        pthread_mutex_unlock(&mutex_tnc_set);
    } else if (!strncasecmp(t, "heard", 4)) {
        ui_get_heard_list(respbuf, respbufsize);
    } else if (!strncasecmp(t, "flist", 4)) {
        t = strtok(NULL, "\0");
        if (t) {
            /* trim leading and trailing spaces */
            while (*t && (*t == ' ' || *t == '/'))
                ++t;
            len = strlen(t);
            if (len) {
                p = t + len - 1;
                while (p > t && *p == ' ') {
                    *p = '\0';
                    --p;
                }
            }
            if (strlen(t) == 0)
                t = NULL;
        }
        /* check to see if this is an access controlled dir */
        snprintf(dpath, sizeof(dpath), "%s/%s", g_arim_settings.files_dir, t);
        if (t && ini_check_ac_files_dir(dpath)) {
            if (arim_is_arq_state()) {
                if (arim_arq_auth_get_status()) {
                    /* session previously authenticated, go ahead */
                    result = ui_get_file_list(g_arim_settings.files_dir, t, respbuf, respbufsize);
                    return (result ? CMDPROC_OK : CMDPROC_DIR_ERR);
                } else {
                    /* session not yet authenticated, send /A1 challenge */
                    arim_copy_remote_call(remote_call, sizeof(remote_call));
                    if (arim_arq_auth_on_send_a1(remote_call, "FLIST", t)) {
                        arim_on_event(EV_ARQ_AUTH_SEND_CMD, 1);
                        snprintf(buffer, sizeof(buffer),
                                    "ARQ: Listing of dir %s requires authentication", t);
                        bufq_queue_debug_log(buffer);
                        return CMDPROC_AUTH_REQ;
                    } else {
                        /* not accessible to remote station, send /EAUTH response */
                        snprintf(buffer, sizeof(buffer),
                                "ARQ: Listing of dir %s no password for: %s", t, remote_call);
                        bufq_queue_debug_log(buffer);
                        return CMDPROC_AUTH_ERR;
                    }
                }
            } else {
                /* access controlled dirs not accessible in FEC mode */
                snprintf(respbuf, respbufsize, "File list: directory %s not found.\n", t);
                return CMDPROC_DIR_ERR;
            }
        } else {
            /* no authentication required */
            result = ui_get_file_list(g_arim_settings.files_dir, t, respbuf, respbufsize);
            return (result ? CMDPROC_OK : CMDPROC_DIR_ERR);
        }
    } else if (!strncasecmp(t, "file", 4)) {
        t = strtok(NULL, "\0");
        if (t) {
            /* trim leading and trailing spaces */
            while (*t && *t == ' ')
                ++t;
            len = strlen(t);
            if (len) {
                p = t + len - 1;
                while (p > t && *p == ' ') {
                    *p = '\0';
                    --p;
                }
            }
            /* check for dynamic file name */
            for (i = 0; i < g_arim_settings.dyn_files_cnt; i++) {
                if (!strncmp(g_arim_settings.dyn_files[i], t, len)) {
                    if (g_arim_settings.dyn_files[i][len] == ':') {
                        result = ui_get_dyn_file(t, &(g_arim_settings.dyn_files[i][len + 1]),
                                                    respbuf, respbufsize);
                        return (result ? CMDPROC_OK : CMDPROC_FILE_ERR);
                    }
                }
            }
            if (i == g_arim_settings.dyn_files_cnt) {
                /* check for directory component in name */
                snprintf(dpath, sizeof(dpath), "%s/%s", g_arim_settings.files_dir, t);
                p = dpath + strlen(dpath);
                while (p > dpath && *p != '/') {
                    *p = '\0';
                    --p;
                }
                if (p > dpath) {
                    /* check to see if this is an access controlled dir */
                    if (ini_check_ac_files_dir(dpath)) {
                        if (arim_is_arq_state()) {
                            if (arim_arq_auth_get_status()) {
                                /* session previously authenticated, go ahead */
                                result = ui_get_file(t, respbuf, respbufsize);
                                return (result ? CMDPROC_OK : CMDPROC_FILE_ERR);
                            } else {
                                /* session not yet authenticated, send /A1 challenge */
                                arim_copy_remote_call(remote_call, sizeof(remote_call));
                                if (arim_arq_auth_on_send_a1(remote_call, "FILE", t)) {
                                    arim_on_event(EV_ARQ_AUTH_SEND_CMD, 1);
                                    snprintf(buffer, sizeof(buffer),
                                                "ARQ: Read of file  %s requires authentication", t);
                                    bufq_queue_debug_log(buffer);
                                    return CMDPROC_AUTH_REQ;
                                } else {
                                    /* no access for remote call, send /EAUTH response */
                                    snprintf(buffer, sizeof(buffer),
                                            "ARQ: Read of file %s no password for: %s", t, remote_call);
                                    bufq_queue_debug_log(buffer);
                                    return CMDPROC_AUTH_ERR;
                                }
                            }
                        } else {
                            /* access controlled dirs not accessible in FEC mode */
                            snprintf(respbuf, respbufsize, "File: %s not found.\n", t);
                            return CMDPROC_FILE_ERR;
                        }
                    } else {
                        /* file not located in access controlled dir */
                        result = ui_get_file(t, respbuf, respbufsize);
                        return (result ? CMDPROC_OK : CMDPROC_FILE_ERR);
                    }
                } else {
                    /* file located in root shared file dir */
                    result = ui_get_file(t, respbuf, respbufsize);
                    return (result ? CMDPROC_OK : CMDPROC_FILE_ERR);
                }
            }
        } else {
            if (!arim_is_arq_state())
                snprintf(respbuf, respbufsize, "Error: empty file name.\n");
            return CMDPROC_FILE_ERR;
        }
    } else if (!strncasecmp(t, "netcalls", 4)) {
        snprintf(respbuf, respbufsize, "NETCALLS:\n");
        len = strlen(respbuf);
        pthread_mutex_lock(&mutex_tnc_set);
        cnt = g_tnc_settings[g_cur_tnc].netcall_cnt;
        for (i = 0; i < cnt; i++) {
            snprintf(buffer, sizeof(buffer), "%s\n", g_tnc_settings[g_cur_tnc].netcall[i]);
            if (len + strlen(buffer) < respbufsize) {
                strncat(respbuf, buffer, respbufsize - len - 1);
                len = strlen(respbuf);
            }
        }
        pthread_mutex_unlock(&mutex_tnc_set);
        if (len + 1 < respbufsize)
            strncat(respbuf, "\n", respbufsize - len - 1);
    } else {
        snprintf(respbuf, respbufsize, "Error: unknown query.\n");
        return CMDPROC_FAIL;
    }
    return CMDPROC_OK;
}
