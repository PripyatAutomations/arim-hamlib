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
#include "ini.h"
#include "main.h"
#include "cmdthread.h"
#include "datathread.h"
#include "serialthread.h"
#include "ini.h"
#include "ui.h"
#include "arim_beacon.h"
#include "tnc_attach.h"
#include "log.h"
#include "arim_arq.h"
#include "arim_proto.h"

TNC_VERSION g_tnc_version;

void tnc_get_version(const char *str)
{
    char *p, *d, verstr[64];
    int i, n;

    memset(&g_tnc_version, 0, sizeof(g_tnc_version));
    /* TNC_2.0.3.9-BPQ ARDOP_2WIN_2.0.4 */
    snprintf(verstr, sizeof(verstr), "%s", str);
    if (strstr(verstr, "BPQ"))
        g_tnc_version.vendor = 'B';
    else
        g_tnc_version.vendor = 'W';
    p = verstr;
    while (!isdigit((int)*p))
        ++p;
    /* at start of version number */
    for (i = 0; *p && i < 3; i++) {
        d = strstr(p, ".");
        if (d) {
            *d = '\0';
            n = atoi(p);
            switch (i) {
            case 0:
                g_tnc_version.major = n;
                break;
            case 1:
                g_tnc_version.minor = n;
                break;
            case 2:
                g_tnc_version.revision = n;
                break;
            }
        } else {
            break;
        }
        ++d;
        p = d;
    }
    /* lastly, the build, if present */
    if (*p) {
        n = atoi(p);
        if (i == 2)
            g_tnc_version.revision = n;
        else
            g_tnc_version.build = n;
    }
}

int tnc_attach_tcp(int which)
{
    int result1, result2 = 0;

    g_cur_tnc = which;
    g_cmdthread_ready = g_datathread_ready = 0;
    g_cmdthread_stop = g_datathread_stop = 0;
    result1 = pthread_create(&g_cmdthread, NULL, cmdthread_func, NULL);
    if (!result1) {
        result2 = pthread_create(&g_datathread, NULL, datathread_func, NULL);
        if (result2) {
            g_cmdthread_stop = 1;
            pthread_join(g_cmdthread, NULL);
            g_cmdthread = 0;
        }
    }
    if (!result1 && !result2) {
        /* while waiting for threads to signal ready status,
           check stop flags and terminate if one or the other is set,
           because thread encountered an error when attempting to connect */
        do {
            if (g_cmdthread_stop) {
                pthread_join(g_cmdthread, NULL);
                g_cmdthread = 0;
                if (!g_datathread_stop) {
                    /* shut down the other thread */
                    g_datathread_stop = 1;
                    pthread_join(g_datathread, NULL);
                    g_datathread = 0;
                    g_datathread_ready = 0;
                }
            }
            if (g_datathread_stop) {
                pthread_join(g_datathread, NULL);
                g_datathread = 0;
                if (!g_cmdthread_stop) {
                    /* shut down the other thread */
                    g_cmdthread_stop = 1;
                    pthread_join(g_cmdthread, NULL);
                    g_cmdthread = 0;
                    g_cmdthread_ready = 0;
                }
            }
            /* are they both terminated? if so return 0 */
            if (!g_cmdthread && !g_datathread) {
                return 0;
            }
        } while (!g_cmdthread_ready || !g_datathread_ready);
        if (g_cmdthread_ready && g_datathread_ready) {
            g_tnc_attached = 1;
            arim_beacon_set(atoi(g_tnc_settings[g_cur_tnc].btime));
            ui_print_status("TNC connection successful", 1);
            return 1;
        } else {
            ui_print_status("Failed to connect to TNC", 1);
            return 0;
        }
    } else {
        g_cmdthread = 0;
        g_datathread = 0;
        ui_print_status("Failed to start TNC service threads", 1);
        return 0;
    }
    return 1;
}

int tnc_detach_tcp()
{
    if (g_cmdthread) {
        g_cmdthread_stop = 1;
        pthread_join(g_cmdthread, NULL);
        g_cmdthread = 0;
    }
    if (g_datathread) {
        g_datathread_stop = 1;
        pthread_join(g_datathread, NULL);
        g_datathread = 0;
    }
    return 1;
}

int tnc_attach_serial(int which)
{
    int result = 0;

    g_cur_tnc = which;
    g_serialthread_ready = 0;
    g_serialthread_stop = 0;
    result = pthread_create(&g_serialthread, NULL, serialthread_func, NULL);
    if (result) {
        g_serialthread_stop = 1;
        pthread_join(g_serialthread, NULL);
        g_serialthread = 0;
    }
    if (!result) {
        /* while waiting for thread to signal ready status,
           check stop flag and terminate if set, because thread
           encountered an error when attempting to connect */
        do {
            if (g_serialthread_stop) {
                pthread_join(g_serialthread, NULL);
                g_serialthread = 0;
                return 0;
            }
        } while (!g_serialthread_ready);
        if (g_serialthread_ready) {
            g_tnc_attached = 1;
            arim_beacon_set(atoi(g_tnc_settings[g_cur_tnc].btime));
            ui_print_status("TNC connection successful", 1);
            return 1;
        } else {
            ui_print_status("Failed to connect to TNC", 1);
            return 0;
        }
    } else {
        g_serialthread = 0;
        ui_print_status("Failed to start TNC service thread", 1);
        return 0;
    }
    return 1;
}

int tnc_detach_serial()
{
    if (g_serialthread) {
        g_serialthread_stop = 1;
        pthread_join(g_serialthread, NULL);
        g_serialthread = 0;
    }
    return 1;
}

int tnc_attach(int which)
{
    int result;

    ////////////////////////////////////
    // Added hamlib - 20230115
    // Connect the TNC's hamlib controls
    // NONE|BUG|ERR|WARN|VERBOSE|TRACE
    rig_set_debug(RIG_DEBUG_WARN);
    rig_load_all_backends();

    int model = g_tnc_settings[which].hamlib_model;
    RIG *rig = g_tnc_settings[which].hamlib_rig = rig_init(model);
//    rig->port = RIG_PORT_NETWORK;
//    rig->ptt_type = RIG_PTT_RIG;
    int rc = rig_open(rig);

    if (rc != RIG_OK) {
      ui_print_status("hamlib_open: error", 1);
    }
    ui_print_status("Rigctl connected", 1);

    ///////////////////////////
        
    /* initialize logging */
    if (!log_init(which)) {
        ui_print_status("Failed to initialize logging", 1);
    }

    if (!strncasecmp(g_tnc_settings[which].interface, "serial", 6))
        result = tnc_attach_serial(which);
    else
        result = tnc_attach_tcp(which);
    if (!result)
        log_close();
    return result;
}

void tnc_detach()
{
    if (arim_is_arq_state()) {
        arim_arq_on_conn_closed();
    } else {
        datathread_cancel_send_data_out();
        ui_status_xfer_end();
    }
    arim_set_state(ST_IDLE);
    if (!strncasecmp(g_tnc_settings[g_cur_tnc].interface, "serial", 6))
        tnc_detach_serial();
    else
        tnc_detach_tcp();

    //////////////////
    // Added hamlib - 20230115
    if (g_tnc_settings[g_cur_tnc].hamlib_rig != NULL) {
       RIG *rig = g_tnc_settings[g_cur_tnc].hamlib_rig;
       rig_close(rig);
       rig_cleanup(rig);
       g_tnc_settings[g_cur_tnc].hamlib_rig = NULL;
    }
    //////////////////

    arim_set_channel_not_busy();
    log_close();
    g_tnc_attached = 0;
    g_cur_tnc = 0;
    ui_set_tnc_detached();
}

