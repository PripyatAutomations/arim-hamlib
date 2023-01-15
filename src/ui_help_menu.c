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
#include <unistd.h>
#include <string.h>
#include <curses.h>
#include "main.h"
#include "arim_proto.h"
#include "ui.h"
#include "ui_recents.h"
#include "ui_ping_hist.h"
#include "ui_conn_hist.h"
#include "ui_file_hist.h"
#include "ui_heard_list.h"
#include "ui_tnc_data_win.h"
#include "ui_tnc_cmd_win.h"

#define MAX_HELP_ROW_SIZE  256
#define HELP_WIN_SCROLL_LEGEND  "Scrolling: UP DOWN PAGEUP PAGEDOWN HOME END, 'q' to quit. "

const char *help[] = {
    "            ARIM Quick Help - press 'q' to quit.",
    "            ------------------------------------",
    "",
    "--- Press <ESC> to abort data transmission in progress  ---",
    "---      and/or reset the [RF CHANNEL BUSY] state.      ---",
    "----- Press <CTRL-X> to disconnect from ARQ session. ------",
    "",
    "FEC mode hot keys:",
    "  Press 'r' to open the Recent Messages view.",
    "  Press 'p' to open the Ping History view.",
    "  Press 'c' to open the Connection History view.",
    "  Press 'l' to open the ARQ File History view.",
    "  Press 'f' to open the FEC Control menu.",
    "  Press 't' to toggle timestamp format (Clock/Elapsed Time).",
    "  Press 'n' to clear the new message and file counters.",
    "  Press 'h' to open this Help view.",
    "",
    "ARQ mode hot keys:",
    "  Press 'f' to open the shared files viewer.",
    "  Press 'o' to open the message outbox viewer.",
    "  Press 'i' to open the message inbox viewer.",
    "  Press 's' to open the sent messages viewer.",
    "  Press 't' to toggle timestamp format (Clock/Elapsed Time).",
    "  Press 'n' to clear the new message and file counters.",
    "  Press '#' to reopen the remote file listing viewer.",
    "  Press 'h' to open this Help view.",
    "",
    "Press Spacebar to access interactive command line:",
    "  Type in the command and press ENTER to execute it.",
    "  To correct mistakes, use the LEFT, RIGHT, HOME, END,",
    "    DELETE and BACKSPACE keys to move the cursor and",
    "    insert or delete characters.",
    "  To recall previously entered commands, use the UP and",
    "    DOWN keys to browse the command history buffer.",
    "  Enter special command '!!' to immediately execute your",
    "    last command.",
    "",
    "TNC config and control commands:",
    "  'att n' to attach to TNC where n is TNC number.",
    "  'det' to detach from TNC.",
    "  'mycall c' to set MYCALL where c is call sign.",
    "  'netcall add c' to add net call where c is call sign.",
    "  'netcall del c' to delete net call where c is call sign.",
    "  'gridsq loc' to set grid square where loc is locator.",
    "  'pname n' to set TNC name where n is name or description.",
    "  'enpingack v' to control whether or not TNC responds to",
    "    pings from another station, where v is 'true' or",
    "    'false' (not case sensitive).",
    "  'listen v' to control whether or not ARIM listens for",
    "    ARQ connection requests or pings from another station,",
    "    where v is 'true' or 'false' (not case sensitive).",
    "  'tncset' to show TNC settings in a pop-up window.",
    "",
    "When attached to TNC, direct special commands as follows:",
    "  Prefix commands to ARDOP TNC with '!', e.g. '!SENDID'.",
    "  Prefix lines for unproto tx with ':', e.g. ':Hello'.",
    "  Prefix ARQ query commands with '/', e.g. '/version'.",
    "",
    "FEC mode beacon commands:",
    "  'btime n' to set beacon period where n is mins.",
    "  'btest' to test beacon",
    "",
    "FEC mode send ping from the prompt:",
    "  'ping c n', where c is call sign and n is number of repeats.",
    "",
    "FEC mode pilot ping commands:",
    "  'pping n', where n is number of pilot ping repeats;",
    "    set to 0 to disable pilot pings.",
    "  'ppthr n', where n is quality threshold for pilot ping acks;",
    "    if threshold not met then message or query is not sent.",
    "  'ppset' to show pilot ping parameters in a pop-up window.",
    "",
    "FEC mode messaging commands:",
    "  'sm call [msg]' to send msg direct via TNC if attached or",
    "    to outbox if not attached. call is station or net call,",
    "    and msg is optional message text entered on the command",
    "    line. If msg not given on the command line, then enter",
    "    message text line-by-line at the command prompt, then",
    "    '/ex' at the start of a new line to finish, or '/can'",
    "    to cancel.",
    "  'cm call' to compose msg and store to outbox. call is",
    "    station or net call. Enter message text line-by-line at",
    "    the command prompt, then '/ex' at the start of a new",
    "    line to finish, or '/can' to cancel.",
    "  'sq call query' to send query, call is station and query",
    "    is one of 'version', 'gridsq', 'info', 'pname', 'heard',",
    "    'flist', 'netcalls' or 'file fn' where fn is file name.",
    "  'li' to open inbox message list, then:",
    "    'rm n' to read, 'km n' to kill, 'sv n fn' to save to",
    "    file, 'fm n call' to forward, 'cf n fl' to clear flag,",
    "    'pm d' to purge old where n is msg nbr, fn is file name,",
    "    call is destination call sign, fl is message flag (R,F,S",
    "    or * for all) and d is age in days. Press 'q' to quit.",
    "  'lo' to open outbox message list, then:",
    "    'rm n' to read, 'km n' to kill or 'sm n' to send,",
    "    'cf n fl' to clear flag, 'pm d' to purge old where n is",
    "    msg nbr, fl is message flag (R,F,S or * for all) and d",
    "    is age in days. Press 'q' to quit.",
    "  'ls' to open sent message list, then:",
    "    'rm n' to read, 'km n' to kill, 'sv n fn' to save to",
    "    file, 'fm n call' to forward, 'cf n fl' to clear flag,",
    "    'pm d' to purge old where n is msg nbr, fn is file name,",
    "    call is destination call sign, fl is message flag (R,F,S",
    "    or * for all) and d is age in days. Press 'q' to quit.",
    "",
    "FEC mode Recent Messages view:",
    "  Press 'r' hot key to open the Recent Messages view, then:",
    "   'rr n' to read, where n is msg nbr in the recents view.",
    "",
    "FEC mode message send repeat commands:",
    "  'srpts n' to set FEC msg repeats, where n is repeat count.",
    "  'ackto n' to set msg ack timeout, where n is time in seconds.",
    "  'fecds v' to enable/disable FEC mode downshift on msg repeat,",
    "    where v is 'true' or 'false' (not case sensitive).",
    "  'srset' to show msg send repeat parameters in a pop-up window.",
    "",
    "FEC mode shared files viewer commands:",
    "  'lf' to open the shared files viewer, then:",
    "    'rf n' to read file, 'sf n call' to send, 'cd n' to change",
    "    directory where n is file or dir nbr, and call is",
    "    destination call sign, 'ri' to read the ARIM configuration,",
    "    file, 'rp' to read the 'arim-digest' password file, 'rt' to",
    "    read the 'arim-themes' UI themes file. Press 'q' to quit.",
    "",
    "ARQ mode commands and queries:",
    "  'conn c n [bw]' to connect where c is call sign of remote",
    "    station, n is number of connection request repeats, and bw",
    "    is an optional ARQBW specifier. This can be one of:",
    "      ARDOP 1.x: 200MAX, 500MAX, 1000MAX, 2000MAX,",
    "                 200FORCED, 500FORCED, 1000FORCED or 2000FORCED.",
    "      ARDOP 2.x: 200, 500, 2500 or 'any'. If 'any', all possible",
    "        ARQBW settings are tried in succession. This is useful if",
    "        the ARQBW setting of the remote station is unknown.",
    "  When connected, text entered at the command prompt will be",
    "    sent to the remote station and also printed to the local",
    "    traffic monitor view and traffic log.",
    "  If connected to an ARIM station, the following query commands,",
    "    prefixed by the '/' character, may be entered at the prompt",
    "    to retrieve information from the remote station:",
    "      '/version' returns the software version numbers for ARIM",
    "        and the ARDOP TNC.",
    "      '/gridsq' returns the Maidenhead locator (gridsquare).",
    "      '/info' returns the ARIM info statement.",
    "      '/pname' returns the ARIM port 'name' for the TNC in use.",
    "      '/heard' returns the ARIM Calls Heard list.",
    "      '/netcalls' returns the ARIM netcall list.",
    "      '/flist [dir]' where dir is an optional directory path,"
    "        returns a listing of files at the remote station.",
    "      '/flget [-z] [dir]', where -z is compression option and dir",
    "        is an optional directory path on the remote station.",
    "        Downloads a directory listing and displays it in the remote",
    "        shared files viewer for easy file reading and downloading.",
    "        If dir is not specified then the default shared files",
    "        at the remote station is listed.",
    "      '/file fn', where fn is a filename from the shared files",
    "        folder on the remote station, or a file path relative to",
    "        that folder; prints the file to the traffic monitor view.",
    "        Works only for text file types.",
    "      '/fget [-z] fn [> dir]', where -z is compression option, fn a",
    "        file in the shared files folder on the remote station or a",
    "        file path relative to that folder; downloads the file to",
    "        the local station. If dir is specified then the file is",
    "        placed in that folder at the local station; if not then it",
    "        is placed in the default 'download' folder. Works for both",
    "        text and binary file types.",
    "      '/fput [-z] fn [> dir]', where -z is compression option, fn",
    "        is a file in the shared files folder on the local station,",
    "        or a file path relative to that folder, and dir is optional",
    "        destination directory specification; uploads the file to",
    "        the remote station. If dir is specified then the file is",
    "        placed in that folder at the remote station; if not then it",
    "        is placed in the default 'download' folder. Works for both",
    "        text and binary file types.",
    "      '/sm [-z][msg]', where -z is compression option and msg is",
    "        optional message text entered on the command line. Sends",
    "        a message to the remote station's inbox. If msg not given",
    "        on the command line, then enter message text line-by-line",
    "        at the command prompt. Enter '/ex' at the start of a new",
    "        line to finish, or '/can' to cancel.",
    "      '/mlist' returns a list of messages in the remote station's",
    "        outbox that are addressed to your station's call sign.",
    "        Requires authentication.",
    "      '/mget [-z][n]', where -z is compression option and n is",
    "        max messages option; downloads up to n messages addressed",
    "        to your station from the remote station's outbox to your",
    "        inbox. Default value of n is 10. Messages are deleted",
    "        from remote station's outbox. Requires authentication.",
    "      '/auth' triggers the mutual authentication process.",
    "    When entering the command, the '/' character must be the",
    "    first one on the line or the command won't be recognized.",
    "  Press CTRL-X to disconnect, or type the special command '/dis'",
    "    at the command prompt and press ENTER.",
    "",
    "ARQ mode messaging commands:",
    "  Press 'o' hot key to open message outbox, then:",
    "    'rm n' to read, 'km n' to kill or 'sm [-z] n' to send,",
    "    'cf n fl' to clear flag, 'pm d' to purge old where -z is",
    "    compression option, n is msg nbr, fl is message flag (R,F,S",
    "    or * for all) and d is age in days. Press 'q' to quit.",
    "  Press 'i' hot key to open message inbox, then:",
    "    'rm n' to read, 'km n' to kill, 'sv n fn' to save to file,",
    "    'fm [-z] n call' to forward, 'cf n fl' to clear flag, 'pm d'",
    "    to purge old where -z is compression option, n is msg nbr",
    "    fn is file name, call is destination call sign, fl is message",
    "    flag (R,F,S or * for all) and d is age in days.",
    "    Press 'q' to quit.",
    "  Press 's' hot key to open sent messages, then:",
    "    'rm n' to read, 'km n' to kill, 'sv n fn' to save to file,",
    "    'fm [-z] n call' to forward, 'cf n fl' to clear flag, 'pm d'",
    "    to purge old where -z is compression option, n is msg nbr",
    "    fn is file name, call is destination call sign, fl is message",
    "    flag (R,F,S or * for all) and d is age in days.",
    "    Press 'q' to quit.",
    "",
    "ARQ mode shared files viewer commands:",
    "  Press 'f' hot key to open the shared files viewer, then:",
    "    'rf n' to read file, 'sf [-z] n [dir]' to send, 'cd n' to",
    "    change directory where -z is compression option, n is file",
    "    or directory nbr, dir is optional destination folder at the",
    "    remote station, 'ri' to read the ARIM configuration file,",
    "    'rp' to read the 'arim-digest' password file, 'rt' to read",
    "    the 'arim-themes' UI themes file. Press 'q' to quit.",
    "",
    "ARQ mode remote shared files viewer commands:",
    "  Use to '/flget' command to open a remote files listing. Then:",
    "    'rf n' to read file, 'gf [-z] n [dir]' to get (download),",
    "    'cd n' to change directory where -z is compression option,",
    "    n is file or directory nbr, dir is optional destination folder",
    "    at the local station. Press 'q' to quit.",
    "  Note: after closing the viewer, press '#' to reopen it.",
    "",
    "ARQ setup commands:",
    "  'arqto n' to set connection timeout, where n is time in seconds.",
    "  'arqbw bw' to set connection bandwidth, where bw is one of:",
    "    ARDOP 1.x: 200MAX, 500MAX, 1000MAX, 2000MAX,",
    "               200FORCED, 500FORCED, 1000FORCED or 2000FORCED.",
    "    ARDOP 2.x: 200, 500 or 2500.",
    "  'arqnegbw v' to control whether or not TNC negotiates ARQ",
    "    connection bandwidth when another station connects, where",
    "    v is 'true' or 'false' (not case sensitive).",
    "  'arqset' to show ARQ parameters in a pop-up window.",
    "",
    "Authentication password commands:",
    "  'passwd client_call server_call password' to add or change",
    "    a password, where client_call is the call sign of the 'client',",
    "    station, and server_call is the the call sign of the 'server'",
    "    station and password is the password, limited to 32 characters",
    "    in length. See the ARIM Help file for details.",
    "  'delpass client_call server_call' to delete a password, where",
    "    client_call is the 'client' station and server_call is the",
    "    'server' station. See the ARIM Help file for details.",
    "",
    "Traffic Monitor scrolling:",
    "  Press UP, DOWN, PAGEUP, PAGEDOWN, HOME or END key to start",
    "  scrolling through the monitor history buffer. The status",
    "  bar will be updated to indicate that scrolling is active.",
    "  Press 'e' to end scrolling. Scrolling will automatically",
    "  time out after 15 seconds if no movement keys are pressed.",
    "",
    "Recent Messages, Connection History and Ping History scrolling:",
    "  Press 'd' to scroll down, 'u' to scroll up.",
    "",
    "Clear screen commands:",
    "  'clrmon' to clear the Traffic Monitor view.",
    "  'clrheard' to clear the Calls Heard view.",
    "  'clrping' to clear the Ping History view.",
    "  'clrconn' to clear the Connection History view.",
    "  'clrfile' to clear the ARQ File History view.",
    "  'clrrec' to clear the Recent Messages view.",
    "",
    "UI theme control:",
    "  'theme tn' to change theme, where tn is the name of the theme.",
    "  The theme name is limited to 15 characters, and is not case",
    "  sensitive. The built-in themes 'dark' and 'light' are always",
    "  available; others may be defined in the arim-themes file.",
    "",
    "ARIM status bar indicator key (ARQ Mode):",
    "-----------------------------------------",
    "  ! ARQ:CALL+ BW S:STATE",
    "",
    "  !       = Input lockout indicator, displayed when busy",
    "  CALL    = Call sign of remote station",
    "  +       = Authenticated session indicator",
    "  BW      = Negotiated ARQ session bandwith",
    "  S       = Session state, one of ISS, IRS, IDLE, or DISC",
    "",
    "  Examples:",
    "",
    "  ! ARQ:NW8L-1 1000 S:IRS         ARQ:NW8L-1+ 500 S:IDLE",
    "",
    "ARIM status bar indicator key (FEC Mode):",
    "-----------------------------------------",
    "  I/B:T/R FECMODE:REPEATS B:MINUTES",
    "",
    "  I/B     = ARIM state, Idle or Busy",
    "  T/R     = TNC state, Transmit or Receive",
    "  FECMODE = Current ARDOP FEC mode",
    "  REPEATS = Number of ARDOP FEC repeats",
    "  B       = Beacon",
    "  MINUTES = Beacon interval time in minutes, or OFF if 0",
    "",
    "  Examples:",
    "",
    "  B:T 4FSK.500.100:0 B:030        I:R 8PSK.1000.100:0 B:OFF",
    "",
    "ARIM/ARDOP frame type key:",
    "--------------------------",
    "  [B] ARIM beacon          [M] ARIM message",
    "  [Q] ARIM query           [R] ARIM response",
    "  [A] ARIM ack             [N] ARIM nak",
    "                           [!] ARIM bad frame",
    "",
    "  [I] ARDOP id             [E] ARDOP bad frame",
    "  [@] ARDOP ARQ            [U] ARDOP FEC (unproto)",
    "  [P] ARDOP ping           [p] ARDOP ping ack",
    "",
    "ARIM frame structure:",
    "---------------------",
    "  |BVV|FCALL|SIZE|GRID|DATA              Beacon",
    "  |MVV|FCALL|TCALL|SIZE|CHECK|DATA       Message",
    "  |AVV|FCALL|TCALL|                      Ack",
    "  |NVV|FCALL|TCALL|                      Nak",
    "  |QVV|FCALL|TCALL|SIZE|CHECK|DATA       Query",
    "  |RVV|FCALL|TCALL|SIZE|CHECK|DATA       Response",
    "",
    "  B,M,A,N,Q,R = frame type",
    "  VV    = ARIM protocol version, (decimal, 2 digits)",
    "  FCALL = sending station call (10 chars max)",
    "  TCALL = destination station call (10 chars max)",
    "  GRID  = grid square, (8 chars max)",
    "  SIZE  = total frame size (hex, 4 digits)",
    "  CHECK = data payload checksum (hex, 4 digits)",
    "  DATA  = data payload (text, 8-bit encoding)",
    "",
    "Copyright (C) 2016-2021 Robert Cunnings NW8L",
    "This program is free software: you can redistribute it and/or",
    "modify it under the terms of the GNU General Public License",
    "as published by the Free Software Foundation, either version 3",
    "of the License, or (at your option) any later version.",
    0,
};

void ui_print_help_title()
{
    int center, start;

    center = (tnc_data_box_w / 2) - 1;
    start = center - 9;
    if (start < 1)
        start = 1;
    mvwprintw(tnc_data_box, tnc_data_box_h - 1, start, " ARIM QUICK HELP ");
    wrefresh(tnc_data_box);
}

void ui_show_help()
{
    WINDOW *help_win, *prev_win;
    char linebuf[MAX_HELP_ROW_SIZE];
    int i, temp, cmd, max_cols, max_help_rows, max_help_lines;
    int cur, top = 0, quit = 0;

    help_win = newwin(tnc_data_box_h - 2, tnc_data_box_w - 2,
                                 tnc_data_box_y + 1, tnc_data_box_x + 1);
    if (!help_win) {
        ui_print_status("Help: failed to create Help window", 1);
        return;
    }
    if (color_code)
        wbkgd(help_win, COLOR_PAIR(7));
    prev_win = ui_set_active_win(help_win);
    max_help_rows = tnc_data_box_h - 2;
    i = 0;
    while (help[i])
        ++i;
    max_help_lines = i;
    max_cols = (tnc_data_box_w - 4) + 1;
    if (max_cols > sizeof(linebuf))
        max_cols = sizeof(linebuf);
    cur = top;
    for (i = 0; i < max_help_rows && cur < max_help_lines; i++) {
        snprintf(linebuf, max_cols, "%s", help[cur++]);
        mvwprintw(help_win, i, 1, linebuf);
    }
    if (show_titles)
        ui_print_help_title();
    wrefresh(help_win);
    status_timer = 1;
    while (!quit) {
        if (status_timer && --status_timer == 0)
            ui_print_status(HELP_WIN_SCROLL_LEGEND, 0);
        cmd = getch();
        switch (cmd) {
        case KEY_HOME:
            top = 0;
            cur = top;
            wclear(help_win);
            for (i = 0; i < max_help_rows && cur < max_help_lines; i++) {
                snprintf(linebuf, max_cols, "%s", help[cur++]);
                mvwprintw(help_win, i, 1, linebuf);
            }
            wrefresh(help_win);
            break;
        case KEY_END:
            if (max_help_lines < max_help_rows)
                break;
            top = max_help_lines - max_help_rows;
            cur = top;
            wclear(help_win);
            for (i = 0; i < max_help_rows && cur < max_help_lines; i++) {
                snprintf(linebuf, max_cols, "%s", help[cur++]);
                mvwprintw(help_win, i, 1, linebuf);
            }
            wrefresh(help_win);
            break;
        case ' ':
        case KEY_NPAGE:
            top += max_help_rows;
            if (top > max_help_lines - 1)
                top = max_help_lines - 1;
            cur = top;
            wclear(help_win);
            for (i = 0; i < max_help_rows && cur < max_help_lines; i++) {
                snprintf(linebuf, max_cols, "%s", help[cur++]);
                mvwprintw(help_win, i, 1, linebuf);
            }
            wrefresh(help_win);
            break;
        case '-':
        case KEY_PPAGE:
            top -= max_help_rows;
            if (top < 0)
                top = 0;
            cur = top;
            wclear(help_win);
            for (i = 0; i < max_help_rows && cur < max_help_lines; i++) {
                snprintf(linebuf, max_cols, "%s", help[cur++]);
                mvwprintw(help_win, i, 1, linebuf);
            }
            wrefresh(help_win);
            break;
        case KEY_UP:
            top -= 1;
            if (top < 0)
                top = 0;
            cur = top;
            wclear(help_win);
            for (i = 0; i < max_help_rows && cur < max_help_lines; i++) {
                snprintf(linebuf, max_cols, "%s", help[cur++]);
                mvwprintw(help_win, i, 1, linebuf);
            }
            wrefresh(help_win);
            break;
        case '\n':
        case KEY_DOWN:
            top += 1;
            if (top > max_help_lines - 1)
                top = max_help_lines - 1;
            cur = top;
            wclear(help_win);
            for (i = 0; i < max_help_rows && cur < max_help_lines; i++) {
                snprintf(linebuf, max_cols, "%s", help[cur++]);
                mvwprintw(help_win, i, 1, linebuf);
            }
            wrefresh(help_win);
            break;
        case 't':
        case 'T':
            if (last_time_heard == LT_HEARD_ELAPSED) {
                last_time_heard = LT_HEARD_CLOCK;
                ui_print_status("Showing clock time in Heard List, press 't' to toggle", 1);
            } else {
                last_time_heard = LT_HEARD_ELAPSED;
                ui_print_status("Showing elapsed time in Heard List, press 't' to toggle", 1);
            }
            ui_refresh_heard_list();
            if (show_ptable)
                ui_refresh_ptable();
            break;
        case 'r':
        case 'R':
            if (show_ptable || show_ctable || show_ftable)
                break;
            if (show_recents) {
                show_recents = 0;
                ui_print_status("Showing TNC cmds, press 'r' to toggle", 1);
                break;
            }
            temp = arim_get_state();
            if (temp != ST_ARQ_CONNECTED &&
                temp != ST_ARQ_IN_CONNECT_WAIT &&
                temp != ST_ARQ_OUT_CONNECT_WAIT) {
                if (!show_recents) {
                    show_recents = 1;
                    ui_print_status("Showing Recents, <SP> 'rr n' read, 'u' or 'd' to scroll, 'r' to toggle", 1);
                }
            } else {
                ui_print_status("Recent Messages view not available in ARQ session", 1);
            }
            break;
        case 'p':
        case 'P':
            if (show_recents || show_ctable || show_ftable)
                break;
            if (show_ptable) {
                show_ptable = 0;
                ui_print_status("Showing TNC cmds, press 'p' to toggle", 1);
                break;
            }
            temp = arim_get_state();
            if (temp != ST_ARQ_CONNECTED &&
                temp != ST_ARQ_IN_CONNECT_WAIT &&
                temp != ST_ARQ_OUT_CONNECT_WAIT) {
                if (!show_ptable) {
                    show_ptable = 1;
                    ui_print_status("Showing Pings, <SP> 'u' or 'd' to scroll, 'p' to toggle", 1);
                }
            } else {
                ui_print_status("Ping History view not available in ARQ session", 1);
            }
            break;
        case 'c':
        case 'C':
            if (show_recents || show_ptable || show_ftable)
                break;
            if (show_ctable) {
                show_ctable = 0;
                ui_print_status("Showing TNC cmds, press 'c' to toggle", 1);
                break;
            }
            temp = arim_get_state();
            if (temp != ST_ARQ_CONNECTED &&
                temp != ST_ARQ_IN_CONNECT_WAIT &&
                temp != ST_ARQ_OUT_CONNECT_WAIT) {
                if (!show_ctable) {
                    show_ctable = 1;
                    ui_print_status("Showing Connections, <SP> 'u' or 'd' to scroll, 'c' to toggle", 1);
                }
            } else {
                ui_print_status("Connection History view not available in ARQ session", 1);
            }
            break;
        case 'l':
        case 'L':
            if (show_recents || show_ptable || show_ctable)
                break;
            if (show_ftable) {
                show_ftable = 0;
                ui_print_status("Showing TNC cmds, press 'l' to toggle", 1);
                break;
            }
            temp = arim_get_state();
            if (temp != ST_ARQ_CONNECTED &&
                temp != ST_ARQ_IN_CONNECT_WAIT &&
                temp != ST_ARQ_OUT_CONNECT_WAIT) {
                if (!show_ftable) {
                    show_ftable = 1;
                    ui_print_status("Showing ARQ File History, <SP> 'u' or 'd' to scroll, 'l' to toggle", 1);
                }
            } else {
                ui_print_status("ARQ File History view not available in ARQ session", 1);
            }
            break;
        case 'd':
            if (show_ptable && ptable_list_cnt) {
                ui_ptable_inc_start_line();
                ui_refresh_ptable();
            }
            else if (show_ctable && ctable_list_cnt) {
                ui_ctable_inc_start_line();
                ui_refresh_ctable();
            }
            else if (show_ftable && ftable_list_cnt) {
                ui_ftable_inc_start_line();
                ui_refresh_ftable();
            }
            else if (show_recents && recents_list_cnt) {
                ui_recents_inc_start_line();
                ui_refresh_recents();
            }
            break;
        case 'u':
            if (show_ptable && ptable_list_cnt) {
                ui_ptable_dec_start_line();
                ui_refresh_ptable();
            }
            else if (show_ctable && ctable_list_cnt) {
                ui_ctable_dec_start_line();
                ui_refresh_ctable();
            }
            else if (show_ftable && ftable_list_cnt) {
                ui_ftable_dec_start_line();
                ui_refresh_ftable();
            }
            else if (show_recents && recents_list_cnt) {
                ui_recents_dec_start_line();
                ui_refresh_recents();
            }
            break;
        case 'q':
        case 'Q':
            delwin(help_win);
            touchwin(tnc_data_box);
            wrefresh(tnc_data_box);
            quit = 1;
            break;
        case 27:
            ui_on_cancel();
            break;
        default:
            ui_print_cmd_in();
            ui_print_recents();
            ui_print_ptable();
            ui_print_ctable();
            ui_print_ftable();
            ui_print_heard_list();
            ui_check_status_dirty();
            break;
        }
        if (g_win_changed)
            quit = 1;
        usleep(100000);
    }
    ui_set_active_win(prev_win);
    if (show_titles)
        ui_print_data_win_title();
}


