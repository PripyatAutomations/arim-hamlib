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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include "main.h"
#include "ini.h"
#include "tnc_attach.h"

#define MAX_INI_LINE_SIZE 256

const char *fecmodes_v1[] = {
    "4FSK.200.50S",
    "4FSK.500.100S",
    "4FSK.500.100",
    "4FSK.2000.600S",
    "4FSK.2000.600",
    "4PSK.200.100S",
    "4PSK.200.100",
    "8PSK.200.100",
    "4PSK.500.100",
    "8PSK.500.100",
    "4PSK.1000.100",
    "8PSK.1000.100",
    "4PSK.2000.100",
    "8PSK.2000.100",
    "16QAM.200.100",
    "16QAM.500.100",
    "16QAM.1000.100",
    "16QAM.2000.100",
    0,
};

const char *fecmodes_v2[] = {
    "4PSK.200.50",
    "4PSK.200.100",
    "16QAM.200.100",
    "4FSK.500.50",
    "4PSK.500.50",
    "16QAMR.500.100",
    "16QAM.500.100",
    "4FSK.1000.50",
    "4PSKR.2500.50",
    "4PSK.2500.50",
    "16QAMR.2500.100",
    "16QAM.2500.100",
    0,
};

const char *baud_rates[] = {
    "9600",
    "19200",
    "38400",
    "57600",
    "115200",
    0,
};

ARIM_SET g_arim_settings;
LOG_SET g_log_settings;
UI_SET g_ui_settings;
TNC_SET g_tnc_settings[TNC_MAX_COUNT];
int g_cur_tnc, g_num_tnc;
char g_arim_path[MAX_DIR_PATH_SIZE];
char g_config_fname[MAX_PATH_SIZE];
char g_print_config_fname[MAX_PATH_SIZE];
int g_config_clo;
FILE *printconf_fp;

int ini_validate_interface(const char *val)
{
    if (!strncasecmp(val, "serial", 6))
        return 1;
    if (!strncasecmp(val, "tcp", 3))
        return 1;
    return 0;
}

int ini_validate_baudrate(const char *rate)
{
    const char *p;
    int i = 0;

    p = baud_rates[i];
    while (p) {
        if (!strncasecmp(baud_rates[i], rate, strlen(rate)))
            return 1;
        p = baud_rates[++i];
    }
    return 0;
}

int ini_validate_ipaddr(const char *addr)
{
    const char *p;
    size_t len;

    /* per IETF RFC 952, RFC 1123 and RFC 2035 */
    len = strlen(addr);
    /* max length is 255 less implied leading and trailing octets */
    if (len > 253)
        return 0;
    len = 0;
    p = addr;
    while (*p) {
        if (*p == '.') {
            /* check last label length */
            if (len > 63)
                return 0;
            /* labels must not end with '-' */
            if (len > 0 && *(p - 1) == '-')
                return 0;
            len = 0;
        } else if (*p == '-') {
            /* labels must not start with '-' */
            if (len == 0)
                return 0;
            ++len;
        } else if (isalnum((int)*p)) {
            ++len;
        } else {
            return 0;
        }
        ++p;
    }
    /* last char must not be '-' or '.' */
    if (*(p - 1) == '-' || *(p - 1) == '.')
        return 0;
    return 1;
}

int ini_validate_mycall(const char *call)
{
    const char *p;
    size_t len;

    len = strlen(call);
    if (len > MAX_TNC_MYCALL_STRLEN || len < 3)
        return 0;

    p = call;
    while (*p && isalnum((int)*p))
        ++p;
    if (*p == '-') {
        /* encountered SSID */
        ++p;
        if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) {
            /* SSID A-Z, 1 char max */
            if (*(p + 1))
                return 0;
        } else if (*p >= '0' && *p <= '9') {
            /* SSID 0-15, 2 chars max */
            ++p;
            if (*p) {
                if (*(p - 1) > '1' || *p < '0' || *p > '5')
                    return 0;
                ++p;
                if (*p)
                    return 0;
            }
        } else {
            return 0;
        }
    } else if (*p || strlen(call) > 7) {
        /* no SSID but encountered illegal char or body of call exceeds 7 chars */
        return 0;
    }
    return 1;
}

int ini_validate_netcall(const char *call)
{
    const char *p;
    size_t len;

    len = strlen(call);
    if (len > MAX_TNC_NETCALL_STRLEN)
        return 0;

    p = call;
    while (*p && !isspace((int)*p) && isprint((int)*p))
        ++p;
    if (*p)
        return 0;
    return 1;
}

int ini_validate_name(const char *name)
{
    const char *p;
    size_t len;

    len = strlen(name);
    if (len > TNC_NAME_SIZE-1)
        return 0;

    p = name;
    while (*p && isprint((int)*p))
        ++p;
    if (*p)
        return 0;
    return 1;
}

int ini_validate_info(const char *name)
{
    const char *p;
    size_t len;

    len = strlen(name);
    if (len > TNC_INFO_SIZE-1)
        return 0;

    p = name;
    while (*p && *p >= ' ')
        ++p;
    if (*p)
        return 0;
    return 1;
}

int ini_validate_fecmode(const char *mode)
{
    const char *p, **modes;
    int i = 0;

    if (g_tnc_version.major <= 1)
        modes = fecmodes_v1;
    else
        modes = fecmodes_v2;
    p = modes[i];
    while (p) {
        if (!strncasecmp(modes[i], mode, strlen(mode)))
            return 1;
        p = modes[++i];
    }
    return 0;
}

int ini_validate_gridsq(const char *gridsq)
{
    size_t len;

    len = strlen(gridsq);
    if (len != 4 && len != 6 && len != 8)
        return 0;
    if ((gridsq[0] < 'A' || gridsq[0] > 'R') && (gridsq[0] < 'a' || gridsq[0] > 'r'))
        return 0;
    if ((gridsq[1] < 'A' || gridsq[1] > 'R') && (gridsq[1] < 'a' || gridsq[1] > 'r'))
        return 0;
    if (!isdigit((int)gridsq[2]) || !isdigit((int)gridsq[3]))
        return 0;
    if (len > 4) {
        if ((gridsq[4] < 'A' || gridsq[4] > 'X') && (gridsq[4] < 'a' || gridsq[4] > 'x'))
            return 0;
        if ((gridsq[5] < 'A' || gridsq[5] > 'X') && (gridsq[5] < 'a' || gridsq[5] > 'x'))
            return 0;
        if (len > 6) {
            if (!isdigit((int)gridsq[6]) || !isdigit((int)gridsq[7]))
                return 0;
        }
    }
    return 1;
}

int ini_validate_arq_bw(const char *val)
{
    if (g_tnc_version.major <= 1) {
        if (!strncasecmp(val, "200MAX", 6))
            return 1;
        if (!strncasecmp(val, "500MAX", 6))
            return 1;
        if (!strncasecmp(val, "1000MAX", 7))
            return 1;
        if (!strncasecmp(val, "2000MAX", 7))
            return 1;
        if (!strncasecmp(val, "200FORCED", 9))
            return 1;
        if (!strncasecmp(val, "500FORCED", 9))
            return 1;
        if (!strncasecmp(val, "1000FORCED", 10))
            return 1;
        if (!strncasecmp(val, "2000FORCED", 10))
            return 1;
    } else {
        if (!strncasecmp(val, "200", 3))
            return 1;
        if (!strncasecmp(val, "500", 3))
            return 1;
        if (!strncasecmp(val, "2500", 4))
            return 1;
    }
    return 0;
}

int ini_validate_bool(const char *val)
{
    if (!strncasecmp(val, "TRUE", 4))
        return 1;
    return 0;
}

char *ini_get_value(const char *key, char *line)
{
    static char *v;
    char *p, *s, *e;

    if ((s = strstr(line, "="))) {
        /* key/value pair, find start of key */
        p = line;
        while (*p && (*p == ' ' || *p == '\t'))
            ++p;
        /* find end of key */
        e = p;
        while (*e && *e != ' ' && *e != '\t' && *e != '=')
            ++e;
        /* test for exact match */
        if (p == strstr(p, key) && strlen(key) == (e - p)) {
            /* matches, find start of value */
            ++s;
            while (*s && (*s == ' ' || *s == '\t'))
                ++s;
            /* find end of value and null-terminate the key/value pair */
            e = s;
            while (*e && *e != '\r' && *e != '\n')
                ++e;
            --e;
            while (*e == ' ' || *e == '\t')
                --e;
            *(e + 1) = '\0';
            v = s;
            return v;
        }
    }
    return NULL;
}

int ini_check_add_files_dir(const char *path)
{
    char allowed_path[MAX_PATH_SIZE], test_path[MAX_DIR_PATH_SIZE];
    int i, wildcard = 0;
    size_t len;

    snprintf(test_path, sizeof(test_path), "%s", path);
    /* don't allow directory traversal */
    if (strstr(test_path, ".."))
        return 0;
    /* trim trailing '/' if present */
    len = strlen(test_path);
    if (test_path[len - 1] == '/')
        test_path[len - 1] = '\0';
    /* iterate over allowed directory paths and check for a match */
    for (i = 0; i < g_arim_settings.add_files_dir_cnt; i++) {
        snprintf(allowed_path, sizeof(allowed_path), "%s/%s",
                g_arim_settings.files_dir, g_arim_settings.add_files_dir[i]);
        len = strlen(allowed_path);
        if (len) {
            /* check for wildcard path spec */
            if (allowed_path[len - 1] == '*') {
                if (len > 1 && allowed_path[len - 2] == '/') {
                    allowed_path[len - 1] = '\0';
                    --len;
                    wildcard = 1;
                } else {
                    /* bad path spec */
                    continue;
                }
            }
        } else {
            continue; /* empty */
        }
        if (wildcard) {
            /* check for match with the stem of the path being tested */
            if (!strncmp(allowed_path, test_path, len))
                return 1;
            /* remove trailing '/' for exact match test that follows */
            allowed_path[len - 1] = '\0';
        }
        /* check for exact match with the path being tested */
        if (!strcmp(allowed_path, test_path))
            return 1;
    }
    return 0;
}

int ini_check_ac_files_dir(const char *path)
{
    char allowed_path[MAX_PATH_SIZE], test_path[MAX_DIR_PATH_SIZE];
    int i, wildcard = 0;
    size_t len;

    snprintf(test_path, sizeof(test_path), "%s", path);
    /* don't allow directory traversal */
    if (strstr(test_path, ".."))
        return 0;
    /* trim trailing '/' if present */
    len = strlen(test_path);
    if (test_path[len - 1] == '/')
        test_path[len - 1] = '\0';
    /* iterate over allowed directory paths and check for a match */
    for (i = 0; i < g_arim_settings.ac_files_dir_cnt; i++) {
        snprintf(allowed_path, sizeof(allowed_path), "%s/%s",
                g_arim_settings.files_dir, g_arim_settings.ac_files_dir[i]);
        len = strlen(allowed_path);
        if (len) {
            /* check for wildcard path spec */
            if (allowed_path[len - 1] == '*') {
                if (len > 1 && allowed_path[len - 2] == '/') {
                    allowed_path[len - 1] = '\0';
                    --len;
                    wildcard = 1;
                } else {
                    /* bad path spec */
                    continue;
                }
            }
        } else {
            continue; /* empty */
        }
        if (wildcard) {
            /* check for match with the stem of the path being tested */
            if (!strncmp(allowed_path, test_path, len))
                return 1;
            /* remove trailing '/' for exact match test that follows */
            allowed_path[len - 1] = '\0';
        }
        /* check for exact match with the path being tested */
        if (!strcmp(allowed_path, test_path))
            return 1;
    }
    return 0;
}

void ini_read_tnc_set(FILE *inifp, int which)
{
    char linebuf[MAX_INI_LINE_SIZE];
    char *p, *v, *home_path;
    int test, numch;
    size_t len;
    DIR *dirp;

    /* populate with default values */
    memset(&g_tnc_settings[which], 0, sizeof(TNC_SET));
    snprintf(g_tnc_settings[which].ipaddr, sizeof(g_tnc_settings[which].ipaddr), DEFAULT_TNC_IPADDR);
    snprintf(g_tnc_settings[which].port, sizeof(g_tnc_settings[which].port), DEFAULT_TNC_PORT);
    snprintf(g_tnc_settings[which].mycall, sizeof(g_tnc_settings[which].mycall), DEFAULT_TNC_MYCALL);
    snprintf(g_tnc_settings[which].netcall[0], sizeof(g_tnc_settings[which].netcall[0]), DEFAULT_TNC_NETCALL);
    snprintf(g_tnc_settings[which].gridsq, sizeof(g_tnc_settings[which].gridsq), DEFAULT_TNC_GRIDSQ);
    snprintf(g_tnc_settings[which].btime, sizeof(g_tnc_settings[which].btime), DEFAULT_TNC_BTIME);
    snprintf(g_tnc_settings[which].fecmode, sizeof(g_tnc_settings[which].fecmode), DEFAULT_TNC_FECMODE);
    snprintf(g_tnc_settings[which].fecid, sizeof(g_tnc_settings[which].fecid), DEFAULT_TNC_FECID);
    snprintf(g_tnc_settings[which].fecrepeats, sizeof(g_tnc_settings[which].fecrepeats), DEFAULT_TNC_FECREPEATS);
    snprintf(g_tnc_settings[which].leader, sizeof(g_tnc_settings[which].leader), DEFAULT_TNC_LEADER);
    snprintf(g_tnc_settings[which].trailer, sizeof(g_tnc_settings[which].trailer), DEFAULT_TNC_TRAILER);
    snprintf(g_tnc_settings[which].squelch, sizeof(g_tnc_settings[which].squelch), DEFAULT_TNC_SQUELCH);
    snprintf(g_tnc_settings[which].busydet, sizeof(g_tnc_settings[which].busydet), DEFAULT_TNC_BUSYDET);
    snprintf(g_tnc_settings[which].busy, sizeof(g_tnc_settings[which].busy), DEFAULT_TNC_BUSY);
    snprintf(g_tnc_settings[which].state, sizeof(g_tnc_settings[which].state), DEFAULT_TNC_STATE);
    snprintf(g_tnc_settings[which].listen, sizeof(g_tnc_settings[which].listen), DEFAULT_TNC_LISTEN);
    snprintf(g_tnc_settings[which].en_pingack, sizeof(g_tnc_settings[which].en_pingack), DEFAULT_TNC_EN_PINGACK);
    snprintf(g_tnc_settings[which].arq_sendcr, sizeof(g_tnc_settings[which].arq_sendcr), DEFAULT_TNC_ARQ_SENDCR);
    snprintf(g_tnc_settings[which].arq_bandwidth, sizeof(g_tnc_settings[which].arq_bandwidth), DEFAULT_TNC_ARQ_BW);
    snprintf(g_tnc_settings[which].arq_timeout, sizeof(g_tnc_settings[which].arq_timeout), DEFAULT_TNC_ARQ_TO);
    snprintf(g_tnc_settings[which].arq_negotiate_bw, sizeof(g_tnc_settings[which].arq_negotiate_bw), DEFAULT_TNC_NEGOTIATE_BW);
    snprintf(g_tnc_settings[which].reset_btime_tx, sizeof(g_tnc_settings[which].reset_btime_tx), DEFAULT_TNC_RESET_BT_TX);
    snprintf(g_tnc_settings[which].interface, sizeof(g_tnc_settings[which].interface), DEFAULT_TNC_INTERFACE);
    snprintf(g_tnc_settings[which].serial_port, sizeof(g_tnc_settings[which].serial_port), DEFAULT_TNC_SERIAL_PORT);
    snprintf(g_tnc_settings[which].serial_baudrate, sizeof(g_tnc_settings[which].serial_baudrate), DEFAULT_TNC_SERIAL_BAUD);
    snprintf(g_tnc_settings[which].debug_en, sizeof(g_tnc_settings[which].debug_en), DEFAULT_TNC_DEBUG_EN);
    snprintf(g_tnc_settings[which].traffic_en, sizeof(g_tnc_settings[which].traffic_en),  DEFAULT_TNC_TRAFFIC_EN);
    snprintf(g_tnc_settings[which].tncpi9k6_en, sizeof(g_tnc_settings[which].tncpi9k6_en),  DEFAULT_TNC_TNCPI9K6_EN);
//    g_tnc_settings[which].hamlib_model = DEFAULT_HAMLIB_MODEL;
    g_tnc_settings[which].hamlib_model = -1;

    home_path = getenv("HOME");
    if (home_path)
        numch = snprintf(g_tnc_settings[which].log_dir, sizeof(g_tnc_settings[which].log_dir), "%s", home_path);

    /* if program invoked with --print-conf switch, print section header */
    if (g_print_config)
        fprintf(printconf_fp ? printconf_fp : stdout, "%s\n", "[tnc]");
    p = fgets(linebuf, sizeof(linebuf), inifp);
    while (p) {
        if (*p != '#') {
            while (*p == ' ' || *p == '\t')
                p++;
            if (*p == '[') {
                fseek(inifp, -(strlen(linebuf)), SEEK_CUR);
                break; /* start of next section, done */
            }
            if ((v = ini_get_value("ipaddr", p))) {
                if (ini_validate_ipaddr(v))
                    snprintf(g_tnc_settings[which].ipaddr, sizeof(g_tnc_settings[which].ipaddr), "%s", v);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "ipaddr", g_tnc_settings[which].ipaddr);
            }
            else if ((v = ini_get_value("port", p))) {
                test = atoi(v);
                if (test > 0 && test < 0xFFFF)
                    snprintf(g_tnc_settings[which].port, sizeof(g_tnc_settings[which].port), "%d", test);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "port", g_tnc_settings[which].port);
            }
            else if ((v = ini_get_value("mycall", p))) {
                if (ini_validate_mycall(v))
                    snprintf(g_tnc_settings[which].mycall, sizeof(g_tnc_settings[which].mycall), "%s", v);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "mycall", g_tnc_settings[which].mycall);
            }
            else if ((v = ini_get_value("netcall", p))) {
                if (ini_validate_netcall(v) && g_tnc_settings[which].netcall_cnt < TNC_NETCALL_MAX_CNT)
                    snprintf(g_tnc_settings[which].netcall[g_tnc_settings[which].netcall_cnt],
                         TNC_NETCALL_SIZE, "%s", v);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "netcall",
                                g_tnc_settings[which].netcall[g_tnc_settings[which].netcall_cnt]);
                ++g_tnc_settings[which].netcall_cnt;
            }
            else if ((v = ini_get_value("gridsq", p))) {
                if (ini_validate_gridsq(v))
                    snprintf(g_tnc_settings[which].gridsq, sizeof(g_tnc_settings[which].gridsq), "%s", v);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "gridsq", g_tnc_settings[which].gridsq);
            }
            else if ((v = ini_get_value("reset-btime-on-tx", p))) {
                if (ini_validate_bool(v))
                    snprintf(g_tnc_settings[which].reset_btime_tx, sizeof(g_tnc_settings[which].reset_btime_tx), "TRUE");
                else
                    snprintf(g_tnc_settings[which].reset_btime_tx, sizeof(g_tnc_settings[which].reset_btime_tx), "FALSE");
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "reset-btime-on-tx", g_tnc_settings[which].reset_btime_tx);
            }
            else if ((v = ini_get_value("btime", p))) {
                test = atoi(v);
                if (test >= 0 && test <= MAX_TNC_BTIME_VALUE)
                    snprintf(g_tnc_settings[which].btime, sizeof(g_tnc_settings[which].btime), "%d", test);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "btime", g_tnc_settings[which].btime);
            }
            else if ((v = ini_get_value("name", p))) {
                if (ini_validate_name(v))
                    snprintf(g_tnc_settings[which].name, sizeof(g_tnc_settings[which].name), "%s", v);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "name", g_tnc_settings[which].name);
            }
            else if ((v = ini_get_value("info", p))) {
                if (ini_validate_info(v))
                    snprintf(g_tnc_settings[which].info, sizeof(g_tnc_settings[which].info), "%s", v);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "info", g_tnc_settings[which].info);
            }
            else if ((v = ini_get_value("fecrepeats", p))) {
                test = atoi(v);
                if (test >= 0 && test <= MAX_TNC_FECREPEATS_VALUE)
                    snprintf(g_tnc_settings[which].fecrepeats, sizeof(g_tnc_settings[which].fecrepeats), "%s", v);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "fecrepeats", g_tnc_settings[which].fecrepeats);
            }
            else if ((v = ini_get_value("fecid", p))) {
                if (ini_validate_bool(v))
                    snprintf(g_tnc_settings[which].fecid, sizeof(g_tnc_settings[which].fecid), "TRUE");
                else
                    snprintf(g_tnc_settings[which].fecid, sizeof(g_tnc_settings[which].fecid), "FALSE");
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "fecid", g_tnc_settings[which].fecid);
            }
            else if ((v = ini_get_value("fecmode", p))) {
                /* TNC version dependent - will be validated after attaching to TNC */
                snprintf(g_tnc_settings[which].fecmode, sizeof(g_tnc_settings[which].fecmode), "%s", v);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "fecmode", g_tnc_settings[which].fecmode);
            }
            else if ((v = ini_get_value("leader", p))) {
                test = atoi(v);
                if (test >= MIN_TNC_LEADER_VALUE && test <= MAX_TNC_LEADER_VALUE)
                    snprintf(g_tnc_settings[which].leader, sizeof(g_tnc_settings[which].leader), "%d", test);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "leader", g_tnc_settings[which].leader);
            }
            else if ((v = ini_get_value("trailer", p))) {
                test = atoi(v);
                if (test >= 0 && test <= MAX_TNC_TRAILER_VALUE)
                    snprintf(g_tnc_settings[which].trailer, sizeof(g_tnc_settings[which].trailer), "%d", test);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "trailer", g_tnc_settings[which].trailer);
            }
            else if ((v = ini_get_value("squelch", p))) {
                test = atoi(v);
                if (test >= MIN_TNC_SQUELCH_VALUE && test <= MAX_TNC_SQUELCH_VALUE)
                    snprintf(g_tnc_settings[which].squelch, sizeof(g_tnc_settings[which].squelch), "%d", test);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "squelch", g_tnc_settings[which].squelch);
            }
            else if ((v = ini_get_value("busydet", p))) {
                test = atoi(v);
                if (test >= MIN_TNC_BUSYDET_VALUE && test <= MAX_TNC_BUSYDET_VALUE)
                    snprintf(g_tnc_settings[which].busydet, sizeof(g_tnc_settings[which].busydet), "%d", test);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "busydet", g_tnc_settings[which].busydet);
            }
            else if ((v = ini_get_value("enpingack", p))) {
                if (ini_validate_bool(v))
                    snprintf(g_tnc_settings[which].en_pingack, sizeof(g_tnc_settings[which].en_pingack), "TRUE");
                else
                    snprintf(g_tnc_settings[which].en_pingack, sizeof(g_tnc_settings[which].en_pingack), "FALSE");
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "enpingack", g_tnc_settings[which].en_pingack);
            }
            else if ((v = ini_get_value("listen", p))) {
                if (ini_validate_bool(v))
                    snprintf(g_tnc_settings[which].listen, sizeof(g_tnc_settings[which].listen), "TRUE");
                else
                    snprintf(g_tnc_settings[which].listen, sizeof(g_tnc_settings[which].listen), "FALSE");
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "listen", g_tnc_settings[which].listen);
            }
            else if ((v = ini_get_value("hamlib-model", p))) {
                test = atoi(v);
                if (test >= 0)
                   g_tnc_settings[which].hamlib_model = test;
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%d\n", "hamlib-model", g_tnc_settings[which].hamlib_model);
            }
            else if ((v = ini_get_value("arq-sendcr", p))) {
                if (ini_validate_bool(v))
                    snprintf(g_tnc_settings[which].arq_sendcr, sizeof(g_tnc_settings[which].arq_sendcr), "TRUE");
                else
                    snprintf(g_tnc_settings[which].arq_sendcr, sizeof(g_tnc_settings[which].arq_sendcr), "FALSE");
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "arq-sendcr", g_tnc_settings[which].arq_sendcr);
            }
            else if ((v = ini_get_value("arq-timeout", p))) {
                test = atoi(v);
                if (test >= MIN_TNC_ARQ_TO && test <= MAX_TNC_ARQ_TO)
                    snprintf(g_tnc_settings[which].arq_timeout, sizeof(g_tnc_settings[which].arq_timeout), "%d", test);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "arq-timeout", g_tnc_settings[which].arq_timeout);
            }
            else if ((v = ini_get_value("arq-bandwidth", p))) {
                /* TNC version dependent - will be validated after attaching to TNC */
                snprintf(g_tnc_settings[which].arq_bandwidth, sizeof(g_tnc_settings[which].arq_bandwidth), "%s", v);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "arq-bandwidth", g_tnc_settings[which].arq_bandwidth);
            }
            else if ((v = ini_get_value("arq-negotiate-bw", p))) {
                if (ini_validate_bool(v))
                    snprintf(g_tnc_settings[which].arq_negotiate_bw, sizeof(g_tnc_settings[which].arq_negotiate_bw), "TRUE");
                else
                    snprintf(g_tnc_settings[which].arq_negotiate_bw, sizeof(g_tnc_settings[which].arq_negotiate_bw), "FALSE");
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "arq-negotiate-bw", g_tnc_settings[which].arq_negotiate_bw);
            }
            else if ((v = ini_get_value("interface", p))) {
                if (ini_validate_interface(v))
                    snprintf(g_tnc_settings[which].interface, sizeof(g_tnc_settings[which].interface), "%s", v);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "interface", g_tnc_settings[which].interface);
            }
            else if ((v = ini_get_value("serial-baudrate", p))) {
                if (ini_validate_baudrate(v))
                    snprintf(g_tnc_settings[which].serial_baudrate, sizeof(g_tnc_settings[which].serial_baudrate), "%s", v);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "serial-baudrate", g_tnc_settings[which].serial_baudrate);
            }
            else if ((v = ini_get_value("serial-port", p))) {
                snprintf(g_tnc_settings[which].serial_port, sizeof(g_tnc_settings[which].serial_port), "%s", v);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "serial-port", g_tnc_settings[which].serial_port);
            }
            else if ((v = ini_get_value("tnc-init-cmd", p))) {
                if (g_tnc_settings[which].tnc_init_cmds_cnt < TNC_INIT_CMDS_MAX_CNT)
                    snprintf(g_tnc_settings[which].tnc_init_cmds[g_tnc_settings[which].tnc_init_cmds_cnt],
                         TNC_INIT_CMD_SIZE, "%s", v);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "tnc-init-cmd",
                                g_tnc_settings[which].tnc_init_cmds[g_tnc_settings[which].tnc_init_cmds_cnt]);
                ++g_tnc_settings[which].tnc_init_cmds_cnt;
            }
            else if ((v = ini_get_value("debug-log", p))) {
                if (ini_validate_bool(v))
                    snprintf(g_tnc_settings[which].debug_en, sizeof(g_tnc_settings[which].debug_en), "TRUE");
                else
                    snprintf(g_tnc_settings[which].debug_en, sizeof(g_tnc_settings[which].debug_en), "FALSE");
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "debug-log", g_tnc_settings[which].debug_en);
            }
            else if ((v = ini_get_value("traffic-log", p))) {
                if (ini_validate_bool(v))
                    snprintf(g_tnc_settings[which].traffic_en, sizeof(g_tnc_settings[which].traffic_en), "TRUE");
                else
                    snprintf(g_tnc_settings[which].traffic_en, sizeof(g_tnc_settings[which].traffic_en), "FALSE");
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "traffic-log", g_tnc_settings[which].traffic_en);
            }
            else if ((v = ini_get_value("tncpi9k6-log", p))) {
                if (ini_validate_bool(v))
                    snprintf(g_tnc_settings[which].tncpi9k6_en, sizeof(g_tnc_settings[which].tncpi9k6_en), "TRUE");
                else
                    snprintf(g_tnc_settings[which].tncpi9k6_en, sizeof(g_tnc_settings[which].tncpi9k6_en), "FALSE");
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "tncpi9k6-log", g_tnc_settings[which].tncpi9k6_en);
            }
            else if ((v = ini_get_value("log-dir", p))) {
                /* may be absolute path; if not make it relative to the root directory */
                if (v[0] == '/') {
                    snprintf(g_tnc_settings[which].log_dir, sizeof(g_tnc_settings[which].log_dir), "%s", v);
                } else {
                    home_path = getenv("HOME");
                    if (home_path) {
                        numch = snprintf(g_tnc_settings[which].log_dir, sizeof(g_tnc_settings[which].log_dir), "%s/%s", home_path, v);
                    }
                }
                /* trim trailing '/' if present */
                len = strlen(g_tnc_settings[which].log_dir);
                if (g_tnc_settings[which].log_dir[len - 1] == '/')
                    g_tnc_settings[which].log_dir[len - 1] = '\0';
                /* test directory */
                dirp = opendir(g_tnc_settings[which].log_dir);
                if (!dirp) {
                    if (errno == ENOENT && mkdir(g_tnc_settings[which].log_dir, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH) == -1)
                        return;
                } else {
                    closedir(dirp);
                }
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "log-dir", g_tnc_settings[which].log_dir);
            }
        }
        p = fgets(linebuf, sizeof(linebuf), inifp);
    }
    g_num_tnc++;
    (void)numch; /* suppress 'assigned but not used' warning for dummy var */
}

int ini_get_tnc_set(const char *fn)
{
    FILE *inifp;
    char *p, linebuf[MAX_INI_LINE_SIZE];
    int which_tnc = 0;

    inifp = fopen(fn, "r");
    if (inifp == NULL)
        return 0;
    p = fgets(linebuf, sizeof(linebuf), inifp);
    while (p) {
        if (*p != '#') {
            while (*p == ' ' || *p == '\t')
                p++;
            if (p == strstr(p, "[tnc]")) {
                ini_read_tnc_set(inifp, which_tnc);
                which_tnc++;
            }
        }
        p = fgets(linebuf, sizeof(linebuf), inifp);
    }
    fclose(inifp);
    return 1;
}

void ini_read_log_set(FILE *inifp)
{
    char linebuf[MAX_INI_LINE_SIZE];
    char *p, *v;

    /* if program invoked with --print-conf switch, print section header */
    if (g_print_config)
        fprintf(printconf_fp ? printconf_fp : stdout, "%s\n", "[log]");
    p = fgets(linebuf, sizeof(linebuf), inifp);
    while (p) {
        if (*p != '#') {
            while (*p == ' ' || *p == '\t')
                p++;
            if (*p == '[') {
                fseek(inifp, -(strlen(linebuf)), SEEK_CUR);
                break; /* start of next section, done */
            }
            if ((v = ini_get_value("debug-log", p))) {
                if (ini_validate_bool(v))
                    snprintf(g_log_settings.debug_en, sizeof(g_log_settings.debug_en), "TRUE");
                else
                    snprintf(g_log_settings.debug_en, sizeof(g_log_settings.debug_en), "FALSE");
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "debug-log", g_log_settings.debug_en);
            } else if ((v = ini_get_value("traffic-log", p))) {
                if (ini_validate_bool(v))
                    snprintf(g_log_settings.traffic_en, sizeof(g_log_settings.traffic_en), "TRUE");
                else
                    snprintf(g_log_settings.traffic_en, sizeof(g_log_settings.traffic_en), "FALSE");
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "traffic-log", g_log_settings.traffic_en);
            } else if ((v = ini_get_value("tncpi9k6-log", p))) {
                if (ini_validate_bool(v))
                    snprintf(g_log_settings.tncpi9k6_en, sizeof(g_log_settings.tncpi9k6_en), "TRUE");
                else
                    snprintf(g_log_settings.tncpi9k6_en, sizeof(g_log_settings.tncpi9k6_en), "FALSE");
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "tncpi9k6-log", g_log_settings.tncpi9k6_en);
            }
        }
        p = fgets(linebuf, sizeof(linebuf), inifp);
    }
}

int ini_get_log_set(const char *fn)
{
    FILE *inifp;
    char *p, linebuf[MAX_INI_LINE_SIZE];

    /* populate with default values */
    memset(&g_log_settings, 0, sizeof(LOG_SET));
    snprintf(g_log_settings.debug_en, sizeof(g_log_settings.debug_en), DEFAULT_LOG_DEBUG_EN);
    snprintf(g_log_settings.traffic_en, sizeof(g_log_settings.traffic_en),  DEFAULT_LOG_TRAFFIC_EN);
    snprintf(g_log_settings.tncpi9k6_en, sizeof(g_log_settings.tncpi9k6_en),  DEFAULT_LOG_TNCPI9K6_EN);

    inifp = fopen(fn, "r");
    if (inifp == NULL)
        return 0;
    p = fgets(linebuf, sizeof(linebuf), inifp);
    while (p) {
        if (*p != '#') {
            while (*p == ' ' || *p == '\t')
                p++;
            if (p == strstr(p, "[log]")) {
                ini_read_log_set(inifp);
                break;
            }
        }
        p = fgets(linebuf, sizeof(linebuf), inifp);
    }
    fclose(inifp);
    return 1;
}

int ini_check_ac_calls(const char *call)
{
    int i;
    size_t len;

    if (g_arim_settings.ac_allow_calls_cnt) {
        for (i = 0; i < g_arim_settings.ac_allow_calls_cnt; i++) {
            len = strlen(g_arim_settings.ac_allow_calls[i]);
            if (g_arim_settings.ac_allow_calls[i][len - 1] == '*') {
                if (!strncasecmp(call, g_arim_settings.ac_allow_calls[i], len - 1))
                    return 1; /* wildcard match */
            } else {
                if (!strcasecmp(call, g_arim_settings.ac_allow_calls[i]))
                    return 1; /* exact match */
            }
        }
        return 0;
    } else if (g_arim_settings.ac_deny_calls_cnt) {
        for (i = 0; i < g_arim_settings.ac_deny_calls_cnt; i++) {
            len = strlen(g_arim_settings.ac_deny_calls[i]);
            if (g_arim_settings.ac_deny_calls[i][len - 1] == '*') {
                if (!strncasecmp(call, g_arim_settings.ac_deny_calls[i], len - 1))
                    return 0; /* wildcard match */
            } else {
                if (!strcasecmp(call, g_arim_settings.ac_deny_calls[i]))
                    return 0; /* exact match */
            }
        }
        return 1;
    } else {
        return 1;
    }
}

void parse_ac_calls(const char *data, char *list, int *cnt)
{
    char temp[MAX_INI_LINE_SIZE];
    char *s, *e, *z;

    snprintf(temp, sizeof(temp), "%s", data);
    z = &temp[strlen(temp)]; /* mark end of list */
    s = temp;
    while (*cnt < ARIM_AC_LIST_MAX_CNT) {
        if (s == z)
            break; /* at end, stop */
        /* skip over whitespace or commas */
        while (s < z && (isspace((int)*s) || *s == ','))
            ++s;
        e = s;
        if (e == z)
            break; /* at end, stop */
        /* skip over call sign, stopping at whitespace, comma or wildcard char */
        while (e < z && !isspace((int)*e) &&  *e != ',')
            ++e;
        *e = '\0';
        if (ini_validate_netcall(s)) {
            snprintf(list + (*cnt * TNC_MYCALL_SIZE), TNC_MYCALL_SIZE, "%s", s);
            ++(*cnt);
        }
        if (e == z)
            break; /* at end, stop */
        ++e;
        s = e;
    }
}

void ini_read_arim_set(FILE *inifp)
{
#ifndef PORTABLE_BIN
    FILE *destfp, *srcfp;
    char file_path[MAX_PATH_SIZE];
#endif
    DIR *dirp;
    char *p, *v, linebuf[MAX_INI_LINE_SIZE];
    size_t len;
    int test, numch;

    /* if program invoked with --print-conf switch, print section header */
    if (g_print_config)
        fprintf(printconf_fp ? printconf_fp : stdout, "%s\n", "[arim]");
    p = fgets(linebuf, sizeof(linebuf), inifp);
    while (p) {
        if (*p != '#') {
            while (*p == ' ' || *p == '\t')
                p++;
            if (*p == '[') {
                fseek(inifp, -(strlen(linebuf)), SEEK_CUR);
                break; /* start of next section, done */
            }
            if ((v = ini_get_value("mycall", p))) {
                if (ini_validate_mycall(v))
                    snprintf(g_arim_settings.mycall, sizeof(g_arim_settings.mycall), "%s", v);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "mycall", g_arim_settings.mycall);
            }
            else if ((v = ini_get_value("send-repeats", p))) {
                test = atoi(v);
                if (test >= 0 && test <= MAX_ARIM_SEND_REPEATS)
                    snprintf(g_arim_settings.send_repeats, sizeof(g_arim_settings.send_repeats), "%d", test);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "send-repeats", g_arim_settings.send_repeats);
            }
            else if ((v = ini_get_value("pilot-ping-thr", p))) {
                test = atoi(v);
                if (test >= MIN_ARIM_PILOT_PING_THR && test <= MAX_ARIM_PILOT_PING_THR)
                    snprintf(g_arim_settings.pilot_ping_thr, sizeof(g_arim_settings.pilot_ping_thr), "%d", test);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "pilot-ping-thr", g_arim_settings.pilot_ping_thr);
            }
            else if ((v = ini_get_value("pilot-ping", p))) {
                test = atoi(v);
                if (test >= MIN_ARIM_PILOT_PING && test <= MAX_ARIM_PILOT_PING)
                    snprintf(g_arim_settings.pilot_ping, sizeof(g_arim_settings.pilot_ping), "%d", test);
                else
                    snprintf(g_arim_settings.pilot_ping, sizeof(g_arim_settings.pilot_ping), "%d", 0);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "pilot-ping", g_arim_settings.pilot_ping);
            }
            else if ((v = ini_get_value("ack-timeout", p))) {
                test = atoi(v);
                if (test >= 0 && test <= MAX_ARIM_ACK_TIMEOUT)
                    snprintf(g_arim_settings.ack_timeout, sizeof(g_arim_settings.ack_timeout), "%d", test);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "ack-timeout", g_arim_settings.ack_timeout);
            }
            else if ((v = ini_get_value("frame-timeout", p))) {
                test = atoi(v);
                if (test >= MIN_ARIM_FRAME_TIMEOUT && test <= MAX_ARIM_FRAME_TIMEOUT)
                    snprintf(g_arim_settings.frame_timeout, sizeof(g_arim_settings.frame_timeout), "%d", test);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "frame-timeout", g_arim_settings.frame_timeout);
            }
            else if ((v = ini_get_value("files-dir", p))) {
                /* may be absolute path; if not make it relative to the ARIM root directory */
                if (v[0] == '/')
                    snprintf(g_arim_settings.files_dir, sizeof(g_arim_settings.files_dir), "%s", v);
                else
                    numch = snprintf(g_arim_settings.files_dir, sizeof(g_arim_settings.files_dir), "%s/%s", g_arim_path, v);
                /* trim trailing '/' if present */
                len = strlen(g_arim_settings.files_dir);
                if (g_arim_settings.files_dir[len - 1] == '/')
                    g_arim_settings.files_dir[len - 1] = '\0';
                /* test directory */
                dirp = opendir(g_arim_settings.files_dir);
                if (!dirp) {
                    if (errno == ENOENT && mkdir(g_arim_settings.files_dir, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH) == -1)
                        return;
                } else {
                    closedir(dirp);
                }
#ifndef PORTABLE_BIN
                /* populate this shared files dir with the 'test.txt' file if not found */
                snprintf(file_path, sizeof(file_path), "%s/%s", g_arim_settings.files_dir, DEFAULT_FILE_FNAME);
                if (access(file_path, F_OK) != 0) {
                    snprintf(file_path, sizeof(file_path), ARIM_FILESDIR "/" DEFAULT_FILE_FNAME);
                    srcfp = fopen(file_path, "r");
                    if (srcfp != NULL) {
                        snprintf(file_path, sizeof(file_path), "%s/%s", g_arim_settings.files_dir, DEFAULT_FILE_FNAME);
                        destfp = fopen(file_path, "w");
                        if (destfp != NULL) {
                            p = fgets(linebuf, sizeof(linebuf), srcfp);
                            while (p) {
                                fprintf(destfp, "%s", linebuf);
                                p = fgets(linebuf, sizeof(linebuf), srcfp);
                            }
                            fclose(destfp);
                            fclose(srcfp);
                        } else {
                            fclose(srcfp);
                        }
                    }
                }
#endif
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "files-dir", g_arim_settings.files_dir);
            }
            else if ((v = ini_get_value("add-files-dir", p))) {
                if (g_arim_settings.add_files_dir_cnt < ARIM_ADD_FILES_DIR_MAX_CNT) {
                    snprintf(g_arim_settings.add_files_dir[g_arim_settings.add_files_dir_cnt],
                         sizeof(g_arim_settings.add_files_dir[0]), "%s", v);
                    /* trim trailing '/' if present */
                    len = strlen(g_arim_settings.add_files_dir[g_arim_settings.add_files_dir_cnt]);
                    if (g_arim_settings.add_files_dir[g_arim_settings.add_files_dir_cnt][len - 1] == '/')
                        g_arim_settings.add_files_dir[g_arim_settings.add_files_dir_cnt][len - 1] = '\0';
                    /* if program invoked with --print-conf switch, print key/value pair */
                    if (g_print_config)
                        fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "add-files-dir",
                                    g_arim_settings.add_files_dir[g_arim_settings.add_files_dir_cnt]);
                    ++g_arim_settings.add_files_dir_cnt;
                }
            }
            else if ((v = ini_get_value("ac-files-dir", p))) {
                if (g_arim_settings.ac_files_dir_cnt < ARIM_AC_FILES_DIR_MAX_CNT) {
                    snprintf(g_arim_settings.ac_files_dir[g_arim_settings.ac_files_dir_cnt],
                         sizeof(g_arim_settings.ac_files_dir[0]), "%s", v);
                    /* trim trailing '/' if present */
                    len = strlen(g_arim_settings.ac_files_dir[g_arim_settings.ac_files_dir_cnt]);
                    if (g_arim_settings.ac_files_dir[g_arim_settings.ac_files_dir_cnt][len - 1] == '/')
                        g_arim_settings.ac_files_dir[g_arim_settings.ac_files_dir_cnt][len - 1] = '\0';
                    if (g_print_config)
                        fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "ac-files-dir",
                                    g_arim_settings.ac_files_dir[g_arim_settings.ac_files_dir_cnt]);
                    ++g_arim_settings.ac_files_dir_cnt;
                }
            }
            else if ((v = ini_get_value("fecmode-downshift", p))) {
                if (ini_validate_bool(v))
                    snprintf(g_arim_settings.fecmode_downshift, sizeof(g_arim_settings.fecmode_downshift), "TRUE");
                else
                    snprintf(g_arim_settings.fecmode_downshift, sizeof(g_arim_settings.fecmode_downshift), "FALSE");
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "fecmode-downshift", g_arim_settings.fecmode_downshift);
            }
            else if ((v = ini_get_value("max-msg-days", p))) {
                test = atoi(v);
                if (test >= MIN_ARIM_MSG_DAYS && test <= MAX_ARIM_MSG_DAYS)
                    snprintf(g_arim_settings.max_msg_days, sizeof(g_arim_settings.max_msg_days), "%d", test);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "max-msg-days", g_arim_settings.max_msg_days);
            }
            else if ((v = ini_get_value("max-file-size", p))) {
                test = atoi(v);
                if (test >= 0 && test <= MAX_FILE_SIZE)
                    snprintf(g_arim_settings.max_file_size, sizeof(g_arim_settings.max_file_size), "%d", test);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "max-file-size", g_arim_settings.max_file_size);
            }
            else if ((v = ini_get_value("msg-trace-en", p))) {
                if (ini_validate_bool(v))
                    snprintf(g_arim_settings.msg_trace_en, sizeof(g_arim_settings.msg_trace_en), "TRUE");
                else
                    snprintf(g_arim_settings.msg_trace_en, sizeof(g_arim_settings.msg_trace_en), "FALSE");
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "msg-trace-en", g_arim_settings.msg_trace_en);
            }
            else if ((v = ini_get_value("dynamic-file", p))) {
                if (g_arim_settings.dyn_files_cnt < ARIM_DYN_FILES_MAX_CNT)
                    snprintf(g_arim_settings.dyn_files[g_arim_settings.dyn_files_cnt],
                         ARIM_DYN_FILES_SIZE, "%s", v);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "dynamic-file",
                                g_arim_settings.dyn_files[g_arim_settings.dyn_files_cnt]);
                ++g_arim_settings.dyn_files_cnt;
            }
            else if ((v = ini_get_value("ac-allow", p))) {
                int start = g_arim_settings.ac_allow_calls_cnt;
                parse_ac_calls(v, (char *)g_arim_settings.ac_allow_calls, &g_arim_settings.ac_allow_calls_cnt);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config) {
                    fprintf(printconf_fp ? printconf_fp : stdout, "ac-allow=");
                    for (; start < g_arim_settings.ac_allow_calls_cnt; start++)
                        fprintf(printconf_fp ? printconf_fp : stdout, "%s ", g_arim_settings.ac_allow_calls[start]);
                    fprintf(printconf_fp ? printconf_fp : stdout, "\n");
                }
            }
            else if ((v = ini_get_value("ac-deny", p))) {
                int start = g_arim_settings.ac_deny_calls_cnt;
                parse_ac_calls(v, (char *)g_arim_settings.ac_deny_calls, &g_arim_settings.ac_deny_calls_cnt);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config) {
                    fprintf(printconf_fp ? printconf_fp : stdout, "ac-deny=");
                    for (; start < g_arim_settings.ac_deny_calls_cnt; start++)
                        fprintf(printconf_fp ? printconf_fp : stdout, "%s ", g_arim_settings.ac_deny_calls[start]);
                    fprintf(printconf_fp ? printconf_fp : stdout, "\n");
                }
            }
        }
        p = fgets(linebuf, sizeof(linebuf), inifp);
    }
    (void)numch; /* suppress 'assigned but not used' warning for dummy var */
}

int ini_get_arim_set(const char *fn)
{
    FILE *inifp;
    char *p, linebuf[MAX_INI_LINE_SIZE];
    int numch;

    /* populate with default values */
    memset(&g_arim_settings, 0, sizeof(ARIM_SET));
    snprintf(g_arim_settings.mycall, sizeof(g_arim_settings.mycall), DEFAULT_ARIM_MYCALL);
    snprintf(g_arim_settings.send_repeats, sizeof(g_arim_settings.send_repeats), DEFAULT_ARIM_SEND_REPEATS);
    snprintf(g_arim_settings.pilot_ping, sizeof(g_arim_settings.pilot_ping), DEFAULT_ARIM_PILOT_PING);
    snprintf(g_arim_settings.pilot_ping_thr, sizeof(g_arim_settings.pilot_ping_thr), DEFAULT_ARIM_PILOT_PING_THR);
    snprintf(g_arim_settings.ack_timeout, sizeof(g_arim_settings.ack_timeout), DEFAULT_ARIM_ACK_TIMEOUT);
    snprintf(g_arim_settings.frame_timeout, sizeof(g_arim_settings.frame_timeout), DEFAULT_ARIM_FRAME_TIMEOUT);
    numch = snprintf(g_arim_settings.files_dir, sizeof(g_arim_settings.files_dir), "%s/%s", g_arim_path, DEFAULT_ARIM_FILES_DIR);
    snprintf(g_arim_settings.max_file_size, sizeof(g_arim_settings.max_file_size), DEFAULT_ARIM_FILES_MAX_SIZE);
    snprintf(g_arim_settings.max_msg_days, sizeof(g_arim_settings.max_msg_days), DEFAULT_ARIM_MSG_MAX_DAYS);
    snprintf(g_arim_settings.fecmode_downshift, sizeof(g_arim_settings.fecmode_downshift), DEFAULT_ARIM_FECMODE_DOWN);
    snprintf(g_arim_settings.msg_trace_en, sizeof(g_arim_settings.msg_trace_en), DEFAULT_ARIM_MSG_TRACE_EN);

    inifp = fopen(fn, "r");
    if (inifp == NULL)
        return 0;
    p = fgets(linebuf, sizeof(linebuf), inifp);
    while (p) {
        if (*p != '#') {
            while (*p == ' ' || *p == '\t')
                p++;
            if (p == strstr(p, "[arim]")) {
                ini_read_arim_set(inifp);
                break;
            }
        }
        p = fgets(linebuf, sizeof(linebuf), inifp);
    }
    fclose(inifp);
    (void)numch; /* suppress 'assigned but not used' warning for dummy var */
    return 1;
}

void ini_read_ui_set(FILE *inifp)
{
    char linebuf[MAX_INI_LINE_SIZE];
    char *p, *v;

    /* if program invoked with --print-conf switch, print section header */
    if (g_print_config)
        fprintf(printconf_fp ? printconf_fp : stdout, "%s\n", "[ui]");
    p = fgets(linebuf, sizeof(linebuf), inifp);
    while (p) {
        if (*p != '#') {
            while (*p == ' ' || *p == '\t')
                p++;
            if (*p == '[') {
                fseek(inifp, -(strlen(linebuf)), SEEK_CUR);
                break; /* start of next section, done */
            }
            if ((v = ini_get_value("show-titles", p))) {
                if (ini_validate_bool(v))
                    snprintf(g_ui_settings.show_titles, sizeof(g_ui_settings.show_titles), "TRUE");
                else
                    snprintf(g_ui_settings.show_titles, sizeof(g_ui_settings.show_titles), "FALSE");
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "show-titles", g_ui_settings.show_titles);
            }
            else if ((v = ini_get_value("last-time-heard", p))) {
                if (!strncasecmp(v, "ELAPSED", 7))
                    snprintf(g_ui_settings.last_time_heard, sizeof(g_ui_settings.last_time_heard), "ELAPSED");
                else
                    snprintf(g_ui_settings.last_time_heard, sizeof(g_ui_settings.last_time_heard), "CLOCK");
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "last-time-heard", g_ui_settings.last_time_heard);
            }
            else if ((v = ini_get_value("mon-timestamp", p))) {
                if (ini_validate_bool(v))
                    snprintf(g_ui_settings.mon_timestamp, sizeof(g_ui_settings.mon_timestamp), "TRUE");
                else
                    snprintf(g_ui_settings.mon_timestamp, sizeof(g_ui_settings.mon_timestamp), "FALSE");
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "mon-timestamp", g_ui_settings.mon_timestamp);
            }
            else if ((v = ini_get_value("color-code", p))) {
                if (ini_validate_bool(v))
                    snprintf(g_ui_settings.color_code, sizeof(g_ui_settings.color_code), "TRUE");
                else
                    snprintf(g_ui_settings.color_code, sizeof(g_ui_settings.color_code), "FALSE");
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "color-code", g_ui_settings.color_code);
            }
            else if ((v = ini_get_value("utc-time", p))) {
                if (ini_validate_bool(v))
                    snprintf(g_ui_settings.utc_time, sizeof(g_ui_settings.utc_time), "TRUE");
                else
                    snprintf(g_ui_settings.utc_time, sizeof(g_ui_settings.utc_time), "FALSE");
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "utc-time", g_ui_settings.utc_time);
            }
            else if ((v = ini_get_value("theme", p))) {
                snprintf(g_ui_settings.theme, sizeof(g_ui_settings.theme), "%s", v);
                /* if program invoked with --print-conf switch, print key/value pair */
                if (g_print_config)
                    fprintf(printconf_fp ? printconf_fp : stdout, "%s=%s\n", "theme", g_ui_settings.theme);
            }
        }
        p = fgets(linebuf, sizeof(linebuf), inifp);
    }
}

int ini_get_ui_set(const char *fn)
{
    FILE *inifp;
    char *p, linebuf[MAX_INI_LINE_SIZE];

    /* populate with default values */
    memset(&g_ui_settings, 0, sizeof(LOG_SET));
    snprintf(g_ui_settings.show_titles, sizeof(g_ui_settings.show_titles), DEFAULT_UI_SHOW_TITLES);
    snprintf(g_ui_settings.last_time_heard, sizeof(g_ui_settings.last_time_heard),  DEFAULT_UI_LAST_TIME_HEARD);
    snprintf(g_ui_settings.mon_timestamp, sizeof(g_ui_settings.mon_timestamp), DEFAULT_UI_MON_TIMESTAMP);
    snprintf(g_ui_settings.color_code, sizeof(g_ui_settings.color_code), DEFAULT_UI_COLOR_CODE);
    snprintf(g_ui_settings.utc_time, sizeof(g_ui_settings.utc_time), DEFAULT_UI_UTC_TIME);
    snprintf(g_ui_settings.theme, sizeof(g_ui_settings.theme), DEFAULT_UI_THEME);

    inifp = fopen(fn, "r");
    if (inifp == NULL)
        return 0;
    p = fgets(linebuf, sizeof(linebuf), inifp);
    while (p) {
        if (*p != '#') {
            while (*p == ' ' || *p == '\t')
                p++;
            if (p == strstr(p, "[ui]")) {
                ini_read_ui_set(inifp);
                break;
            }
        }
        p = fgets(linebuf, sizeof(linebuf), inifp);
    }
    fclose(inifp);
    return 1;
}

int ini_read_settings()
{
    int result, numch;
#ifndef PORTABLE_BIN
    FILE *inifp, *srcfp;
    char *p, *home_path, linebuf[MAX_INI_LINE_SIZE];
    DIR *dirp;
#else
    char *cwd_path;
#endif

#ifndef PORTABLE_BIN
    home_path = getenv("HOME");
    if (!home_path)
        return 0;
    snprintf(g_arim_path, sizeof(g_arim_path), "%s/%s", home_path, ARIM_NAME);
    dirp = opendir(g_arim_path);
    if (!dirp) {
        /* create local directory structure for this user */
        if (errno == ENOENT && mkdir(g_arim_path, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH) == -1)
            return 0;
        /* make symlinks to NEWS and PDF Help file */
        numch = snprintf(linebuf, sizeof(linebuf), "%s/doc", g_arim_path);
        if (mkdir(linebuf, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH) == -1)
            return 0;
        numch = snprintf(linebuf, sizeof(linebuf), "%s/doc/NEWS", g_arim_path);
        symlink(ARIM_DOCDIR "/NEWS", linebuf);
        numch = snprintf(linebuf, sizeof(linebuf), "%s/doc/%s", g_arim_path, DEFAULT_PDF_HELP_FNAME);
        symlink(ARIM_DOCDIR "/" DEFAULT_PDF_HELP_FNAME, linebuf);
    } else {
        closedir(dirp);
    }
#else
    cwd_path = getenv("PWD");
    if (!cwd_path)
        return 0;
    snprintf(g_arim_path, sizeof(g_arim_path), "%s", cwd_path);
#endif
    if (!g_config_clo) /* config file name not specified on command line */
        numch = snprintf(g_config_fname, sizeof(g_config_fname), "%s/%s", g_arim_path, DEFAULT_INI_FNAME);
    if (access(g_config_fname, F_OK) != 0) {
#ifndef PORTABLE_BIN
        if (g_config_clo)  /* if config file specified on command line, fail */
            return 0;
        if (errno == ENOENT) {
            inifp = fopen(g_config_fname, "w");
            if (inifp == NULL)
                return 0;
            snprintf(g_config_fname, sizeof(g_config_fname), ARIM_FILESDIR "/" DEFAULT_INI_FNAME);
            srcfp = fopen(g_config_fname, "r");
            if (srcfp == NULL) {
                fclose(inifp);
                return 0;
            }
            p = fgets(linebuf, sizeof(linebuf), srcfp);
            while (p) {
                fprintf(inifp, "%s", linebuf);
                p = fgets(linebuf, sizeof(linebuf), srcfp);
            }
            fclose(srcfp);
            fclose(inifp);
            g_new_install = 1;
        }
#else
        return 0;
#endif
    }
    result = 1;
    if (g_print_config) {
        /* if program invoked with --print-conf switch, print header */
        printconf_fp = fopen(g_print_config_fname, "w");
        fprintf(printconf_fp ? printconf_fp : stdout,
                "\n==== Start ARIM Config File Listing: %s ====\n", g_config_fname);
    }
    if (!ini_get_tnc_set(g_config_fname) || !ini_get_log_set(g_config_fname) ||
        !ini_get_arim_set(g_config_fname) || !ini_get_ui_set(g_config_fname)
       )
        result = 0;
    if (g_print_config) {
        /* if program invoked with --print-conf switch, print trailer */
        fprintf(printconf_fp ? printconf_fp : stdout,
                "==== End ARIM Config File Listing: %s ====\n\n", g_config_fname);
        if (printconf_fp)
            fclose(printconf_fp);
    }
    (void)numch; /* suppress 'assigned but not used' warning for dummy var */
    return result;
}

