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
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "main.h"
#include "arim_proto.h"
#include "ui.h"
#include "log.h"
#include "auth.h"
#include "bufq.h"
#include "arim_arq.h"

static int arq_auth_session_status;
static char ha1[AUTH_BUFFER_SIZE], ha2[AUTH_BUFFER_SIZE];
static char cnonce[AUTH_BUFFER_SIZE], snonce[AUTH_BUFFER_SIZE];
static char fpath[MAX_PATH_SIZE], fmethod[MAX_METHOD_SIZE];

void arim_arq_auth_set_status(int val) {
    arq_auth_session_status = val;
}

int arim_arq_auth_get_status(void) {
    return arq_auth_session_status ? 1 : 0;
}

void arim_arq_auth_on_ok(void)
{
    char linebuf[MAX_LOG_LINE_SIZE];

    arim_arq_auth_set_status(1);
    pthread_mutex_lock(&mutex_tnc_set);
    snprintf(linebuf, sizeof(linebuf), "ARQ: Authentication with %s succeeded",
                g_tnc_settings[g_cur_tnc].arq_remote_call);
    pthread_mutex_unlock(&mutex_tnc_set);
    bufq_queue_debug_log(linebuf);
}

void arim_arq_auth_on_error(void)
{
    char linebuf[MAX_LOG_LINE_SIZE];

    arim_arq_auth_set_status(0);
    pthread_mutex_lock(&mutex_tnc_set);
    snprintf(linebuf, sizeof(linebuf), "ARQ: Authentication with %s failed",
                g_tnc_settings[g_cur_tnc].arq_remote_call);
    pthread_mutex_unlock(&mutex_tnc_set);
    bufq_queue_debug_log(linebuf);
}

void arim_arq_auth_set_ha2_info(const char *method, const char *path)
{
    char linebuf[MAX_LOG_LINE_SIZE];

    snprintf(fpath, sizeof(fpath), "%s", path);
    snprintf(fmethod, sizeof(fmethod), "%s", method);
    snprintf(linebuf, sizeof(linebuf), "AUTH: Caching HA2 string %s", ha2);
    bufq_queue_debug_log(linebuf);
}

int arim_arq_auth_on_send_a1(const char *call, const char *method, const char *path)
{
    char linebuf[MAX_LOG_LINE_SIZE], mycall[TNC_MYCALL_SIZE];
    char remote_call[TNC_MYCALL_SIZE];
    size_t i, len;

    arim_copy_mycall(mycall, sizeof(mycall));
    len = strlen(mycall);
    for (i = 0; i < len; i++)
        mycall[i] = toupper(mycall[i]);
    snprintf(remote_call, sizeof(remote_call), "%s", call);
    len = strlen(remote_call);
    for (i = 0; i < len; i++)
        remote_call[i] = toupper(remote_call[i]);
    /* retrieve HA1 from password file */
    if (!auth_check_passwd(remote_call, mycall, ha1, sizeof(ha1))) {
        snprintf(linebuf, sizeof(linebuf),
            "AUTH: No entry for call %s in arim-digest file)", remote_call);
        bufq_queue_debug_log(linebuf);
        return 0;
    }
    snprintf(linebuf, sizeof(linebuf),
        "AUTH: Found HA1 for call %s in arim-digest file (%s)",
            remote_call, ha1);
    bufq_queue_debug_log(linebuf);
    /* generate HA2 from method and path info */
    snprintf(linebuf, sizeof(linebuf), "%s:%s", method, path);
    auth_b64_digest(AUTH_HA2_DIG_SIZE, (const unsigned char *)linebuf,
                       strlen(linebuf), ha2, sizeof(ha2));
    snprintf(linebuf, sizeof(linebuf),
                "AUTH: Caching HA2 string H(%s:%s)", method, path);
    bufq_queue_debug_log(linebuf);
    /* compute and cache server nonce */
    auth_b64_nonce(snonce, sizeof(snonce));
    /* send auth challenge */
    snprintf(linebuf, sizeof(linebuf), "/A1 %s", snonce);
    arim_arq_send_remote(linebuf);
    snprintf(linebuf, sizeof(linebuf),
                "ARQ: Authentication required for access to %s by %s",
                    path, call);
    bufq_queue_debug_log(linebuf);
    snprintf(linebuf, sizeof(linebuf),
                "AUTH: Sending challenge with snonce %s", snonce);
    bufq_queue_debug_log(linebuf);
    /* prime buffer count because update from TNC not immediate */
    pthread_mutex_lock(&mutex_tnc_set);
    snprintf(g_tnc_settings[g_cur_tnc].buffer,
        sizeof(g_tnc_settings[g_cur_tnc].buffer), "%zu", strlen(linebuf));
    pthread_mutex_unlock(&mutex_tnc_set);
    return 1;
}

int arim_arq_auth_on_send_a2()
{
    char linebuf[MAX_LOG_LINE_SIZE], resp[AUTH_BUFFER_SIZE];
    int numch;

    /* generate HA2 from method and path info */
    numch = snprintf(linebuf, sizeof(linebuf), "%s:%s", fmethod, fpath);
    auth_b64_digest(AUTH_HA2_DIG_SIZE, (const unsigned char *)linebuf,
                       strlen(linebuf), ha2, sizeof(ha2));
    numch = snprintf(linebuf, sizeof(linebuf),
                     "AUTH: Caching HA2 string (%s:%s)", fmethod, fpath);
    bufq_queue_debug_log(linebuf);
    /* compute response */
    numch = snprintf(linebuf, sizeof(linebuf), "%s:%s:%s", ha1, snonce, ha2);
    auth_b64_digest(AUTH_RESP_DIG_SIZE, (const unsigned char *)linebuf,
                       strlen(linebuf), resp, sizeof(resp));
    numch = snprintf(linebuf, sizeof(linebuf),
                     "AUTH: Sending response string H(%s:%s:%s)", ha1, snonce, ha2);
    if (numch >= sizeof(linebuf))
        ui_truncate_line(linebuf, sizeof(linebuf));
    bufq_queue_debug_log(linebuf);
    /* compute and cache client nonce */
    auth_b64_nonce(cnonce, sizeof(cnonce));
    snprintf(linebuf, sizeof(linebuf), "AUTH: Sending client nonce %s", cnonce);
    bufq_queue_debug_log(linebuf);
    /* send the response */
    snprintf(linebuf, sizeof(linebuf), "/A2 %s %s", resp, cnonce);
    arim_arq_send_remote(linebuf);
    snprintf(linebuf, sizeof(linebuf), "ARQ: Sending /A2 response");
    bufq_queue_debug_log(linebuf);
    /* prime buffer count because update from TNC not immediate */
    pthread_mutex_lock(&mutex_tnc_set);
    snprintf(g_tnc_settings[g_cur_tnc].buffer,
        sizeof(g_tnc_settings[g_cur_tnc].buffer), "%zu", strlen(linebuf));
    pthread_mutex_unlock(&mutex_tnc_set);
    return 1;
}

int arim_arq_auth_on_send_a3()
{
    char linebuf[MAX_LOG_LINE_SIZE], resp[AUTH_BUFFER_SIZE];
    int numch;

    /* compute response */
    numch = snprintf(linebuf, sizeof(linebuf), "%s:%s:%s", ha1, cnonce, ha2);
    auth_b64_digest(AUTH_RESP_DIG_SIZE, (const unsigned char *)linebuf,
                       strlen(linebuf), resp, sizeof(resp));
    numch = snprintf(linebuf, sizeof(linebuf),
                     "AUTH: Sending response string H(%s:%s:%s)", ha1, cnonce, ha2);
    if (numch >= sizeof(linebuf))
        ui_truncate_line(linebuf, sizeof(linebuf));
    bufq_queue_debug_log(linebuf);
    /* send the response */
    snprintf(linebuf, sizeof(linebuf), "/A3 %s", resp);
    arim_arq_send_remote(linebuf);
    snprintf(linebuf, sizeof(linebuf), "ARQ: Sending /A3 response");
    bufq_queue_debug_log(linebuf);
    /* prime buffer count because update from TNC not immediate */
    pthread_mutex_lock(&mutex_tnc_set);
    snprintf(g_tnc_settings[g_cur_tnc].buffer,
        sizeof(g_tnc_settings[g_cur_tnc].buffer), "%zu", strlen(linebuf));
    pthread_mutex_unlock(&mutex_tnc_set);
    return 1;
}

int arim_arq_auth_on_a1(char *cmd, size_t size, char *eol)
{
    char *p_nonce, *s, *e;
    char remote_call[TNC_MYCALL_SIZE], mycall[TNC_MYCALL_SIZE];
    char linebuf[MAX_LOG_LINE_SIZE];
    size_t i, len;

    bufq_queue_debug_log("ARQ: processing /A1 command");

    /* inbound auth challenge, get server nonce */
    p_nonce = NULL;
    s = cmd + 4;
    while (*s && *s == ' ')
        ++s;
    p_nonce = s;
    if (*p_nonce && eol) {
        /* trim trailing spaces from nonce token */
        e = eol - 1;
        while (e > p_nonce && *e == ' ') {
            *e = '\0';
            --e;
        }
        /* cache server nonce */
        if (strlen(p_nonce) == AUTH_NONCE_B64_SIZE) {
            snprintf(snonce, sizeof(snonce), "%s", p_nonce);
            snprintf(linebuf, sizeof(linebuf),
                        "AUTH: Received server nonce %s", snonce);
            bufq_queue_debug_log(linebuf);
        } else {
            snprintf(linebuf, sizeof(linebuf), "ARQ: Bad /A1 command");
            bufq_queue_debug_log(linebuf);
            snprintf(linebuf, sizeof(linebuf), "/EAUTH");
            arim_arq_send_remote(linebuf);
            arim_on_event(EV_ARQ_AUTH_ERROR, 0);
            return 0;
        }
        /* get local and remote call signs, force to upper case */
        arim_copy_mycall(mycall, sizeof(mycall));
        len = strlen(mycall);
        for (i = 0; i < len; i++)
            mycall[i] = toupper(mycall[i]);
        arim_copy_remote_call(remote_call, sizeof(remote_call));
        len = strlen(remote_call);
        for (i = 0; i < len; i++)
            remote_call[i] = toupper(remote_call[i]);
        /* retrieve HA1 from password file */
        if (!auth_check_passwd(mycall, remote_call, ha1, sizeof(ha1))) {
            snprintf(linebuf, sizeof(linebuf),
                "AUTH: No entry for call %s in arim-digest file", mycall);
            bufq_queue_debug_log(linebuf);
            snprintf(linebuf, sizeof(linebuf), "/EAUTH");
            arim_arq_send_remote(linebuf);
            arim_on_event(EV_ARQ_AUTH_ERROR, 0);
            return 0;
        }
        snprintf(linebuf, sizeof(linebuf),
                    "AUTH: Found HA1 for call %s in arim-digest file (%s)",
                        remote_call, ha1);
        bufq_queue_debug_log(linebuf);
        /* send a2 response/challenge */
        arim_on_event(EV_ARQ_AUTH_SEND_CMD, 2);
    } else {
        /* bad challenge */
        snprintf(linebuf, sizeof(linebuf), "ARQ: Bad /A1 command");
        bufq_queue_debug_log(linebuf);
        snprintf(linebuf, sizeof(linebuf), "/EAUTH");
        arim_arq_send_remote(linebuf);
        arim_on_event(EV_ARQ_AUTH_ERROR, 0);
        return 0;
    }
    return 1;
}

int arim_arq_auth_on_a2(char *cmd, size_t size, char *eol)
{
    char *p_a1_resp, *p_nonce, *s, *e;
    char linebuf[MAX_LOG_LINE_SIZE], calc_resp[AUTH_BUFFER_SIZE];
    int numch;

    bufq_queue_debug_log("ARQ: processing /A2 command");

    /* inbound auth response + challenge, get response and cnonce */
    p_a1_resp = p_nonce = NULL;
    s = cmd + 4;
    while (*s && *s == ' ')
        ++s;
    p_a1_resp = s;
    if (*p_a1_resp && eol) {
        /* check for nonce token argument */
        while (*s && *s != ' ')
            ++s;
        *s = '\0';
        ++s;
        p_nonce = s;
        if (*p_nonce) {
            while (*s && *s != ' ')
                ++s;
            *s = '\0';
        }
        /* trim trailing spaces from nonce token */
        e = eol - 1;
        while (e > p_nonce && *e == ' ') {
            *e = '\0';
            --e;
        }
        if (strlen(p_nonce) == AUTH_NONCE_B64_SIZE &&
            strlen(p_a1_resp) == AUTH_RESP_B64_SIZE) {
            /* check response */
            numch = snprintf(linebuf, sizeof(linebuf), "%s:%s:%s", ha1, snonce, ha2);
            auth_b64_digest(AUTH_RESP_DIG_SIZE, (const unsigned char *)linebuf,
                               strlen(linebuf), calc_resp, sizeof(calc_resp));
            numch = snprintf(linebuf, sizeof(linebuf),
                             "AUTH: HA1, snonce, HA2 are %s, %s, %s", ha1, snonce, ha2);
            if (numch >= sizeof(linebuf))
                ui_truncate_line(linebuf, sizeof(linebuf));
            bufq_queue_debug_log(linebuf);
            snprintf(linebuf, sizeof(linebuf),
                        "AUTH: Calc response, actual response are %s, %s",
                            calc_resp, p_a1_resp);
            bufq_queue_debug_log(linebuf);
            if (strcmp(calc_resp, p_a1_resp)) {
                /* bad response */
                snprintf(linebuf, sizeof(linebuf), "ARQ: Bad response to /A1 challenge");
                bufq_queue_debug_log(linebuf);
                snprintf(linebuf, sizeof(linebuf), "/EAUTH");
                arim_arq_send_remote(linebuf);
                arim_on_event(EV_ARQ_AUTH_ERROR, 0);
                return 0;
            }
            /* cache client nonce */
            snprintf(cnonce, sizeof(cnonce), "%s", p_nonce);
            snprintf(linebuf, sizeof(linebuf), "AUTH: Received client nonce %s", cnonce);
            bufq_queue_debug_log(linebuf);
            /* send a3 response/challenge */
            arim_on_event(EV_ARQ_AUTH_SEND_CMD, 3);
        } else {
            snprintf(linebuf, sizeof(linebuf), "ARQ: Bad response to /A1 challenge");
            bufq_queue_debug_log(linebuf);
            snprintf(linebuf, sizeof(linebuf), "/EAUTH");
            arim_arq_send_remote(linebuf);
            arim_on_event(EV_ARQ_AUTH_ERROR, 0);
            return 0;
        }
    } else {
        snprintf(linebuf, sizeof(linebuf), "ARQ: Bad /A2 auth command");
        bufq_queue_debug_log(linebuf);
        snprintf(linebuf, sizeof(linebuf), "/EAUTH");
        arim_arq_send_remote(linebuf);
        arim_on_event(EV_ARQ_AUTH_ERROR, 0);
        return 0;
    }
    (void)numch; /* suppress 'assigned but not used' warning for dummy var */
    return 1;
}

int arim_arq_auth_on_a3(char *cmd, size_t size, char *eol)
{
    char *p_a2_resp, *s, *e;
    char linebuf[MAX_LOG_LINE_SIZE], calc_resp[AUTH_BUFFER_SIZE];
    int numch;

    bufq_queue_debug_log("ARQ: processing /A3 command");

    /* inbound auth response, get response token */
    p_a2_resp = NULL;
    s = cmd + 4;
    while (*s && *s == ' ')
        ++s;
    p_a2_resp = s;
    if (*p_a2_resp && eol) {
        /* trim trailing spaces from response token */
        e = eol - 1;
        while (e > p_a2_resp && *e == ' ') {
            *e = '\0';
            --e;
        }
        if (strlen(p_a2_resp) == AUTH_RESP_B64_SIZE) {
            /* check response */
            numch = snprintf(linebuf, sizeof(linebuf), "%s:%s:%s", ha1, cnonce, ha2);
            auth_b64_digest(AUTH_RESP_DIG_SIZE, (const unsigned char *)linebuf,
                               strlen(linebuf), calc_resp, sizeof(calc_resp));
            numch = snprintf(linebuf, sizeof(linebuf),
                         "AUTH: HA1, cnonce, HA2 are %s, %s, %s", ha1, cnonce, ha2);
            if (numch >= sizeof(linebuf))
                ui_truncate_line(linebuf, sizeof(linebuf));
            bufq_queue_debug_log(linebuf);
            snprintf(linebuf, sizeof(linebuf),
                         "AUTH: Calc response, actual response are %s, %s",
                             calc_resp, p_a2_resp);
            bufq_queue_debug_log(linebuf);

            if (strcmp(calc_resp, p_a2_resp)) {
                /* bad response */
                snprintf(linebuf, sizeof(linebuf), "ARQ: Bad response to /A2 challenge");
                bufq_queue_debug_log(linebuf);
                snprintf(linebuf, sizeof(linebuf), "/EAUTH");
                arim_arq_send_remote(linebuf);
                arim_on_event(EV_ARQ_AUTH_ERROR, 0);
                return 0;
            }
            /*  authentication successful, change state */
            arim_arq_auth_on_ok();
            arim_set_state(ST_ARQ_CONNECTED);
            /* replay original command */
            ui_set_status_dirty(STATUS_ARQ_RUN_CACHED_CMD);
        } else {
            snprintf(linebuf, sizeof(linebuf), "ARQ: Bad response to /A2 challenge");
            bufq_queue_debug_log(linebuf);
            snprintf(linebuf, sizeof(linebuf), "/EAUTH");
            arim_arq_send_remote(linebuf);
            arim_on_event(EV_ARQ_AUTH_ERROR, 0);
            return 0;
        }
    } else {
        snprintf(linebuf, sizeof(linebuf), "ARQ: Bad /A3 auth command");
        bufq_queue_debug_log(linebuf);
        snprintf(linebuf, sizeof(linebuf), "/EAUTH");
        arim_arq_send_remote(linebuf);
        arim_on_event(EV_ARQ_AUTH_ERROR, 0);
        return 0;
    }
    return 1;
}

int arim_arq_auth_on_client_challenge(char *cmd)
{
    char *s, *e;
    char salt[MAX_DIR_PATH_SIZE], linebuf[MAX_LOG_LINE_SIZE];
    char mycall[TNC_MYCALL_SIZE], remote_call[TNC_MYCALL_SIZE];

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
    s = salt + 5;
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
    /* cache the /OK response which will be sent when auth is done */
    arim_arq_cache_cmd("/OK");
    arim_arq_auth_set_ha2_info("AUTH", s);
    arim_arq_send_remote(cmd);
    return 1;
}

int arim_arq_auth_on_challenge(char *cmd, size_t size, char *eol)
{
    char *p_salt, *s, *e;
    char linebuf[MAX_LOG_LINE_SIZE], remote_call[TNC_MYCALL_SIZE];

    /* empty outbound data buffer before handling request */
    while (arim_get_buffer_cnt() > 0)
        sleep(1);
    /* parse any "salt" parameter to be used as the HA2 "URL" component */
    s = cmd + 5;
    while (*s && *s == ' ')
        ++s;
    p_salt = s;
    if (*p_salt && eol) {
        /* trim trailing spaces */
        e = eol - 1;
        while (e > p_salt && *e == ' ') {
            *e = '\0';
            --e;
        }
    }
    if (!arim_arq_auth_get_status()) {
        /* send /A1 challenge */
        arim_copy_remote_call(remote_call, sizeof(remote_call));
        if (arim_arq_auth_on_send_a1(remote_call, "AUTH", p_salt)) {
            arim_on_event(EV_ARQ_AUTH_SEND_CMD, 1);
        } else {
            /* no access for remote call, send /ERROR response */
            snprintf(linebuf, sizeof(linebuf), "/EAUTH");
            arim_arq_send_remote(linebuf);
            snprintf(linebuf, sizeof(linebuf), "AUTH: Cannot authenticate %s", remote_call);
            bufq_queue_debug_log(linebuf);
        }
    } else {
        /* session previously authenticated */
        snprintf(linebuf, sizeof(linebuf), "/OK");
        arim_arq_send_remote(linebuf);
    }
    return 1;
}

