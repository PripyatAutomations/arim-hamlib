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

#ifndef _ARIM_PROTO_ARQ_CONN_H_INCLUDED_
#define _ARIM_PROTO_ARQ_CONN_H_INCLUDED_

extern void arim_proto_arq_conn_pend_wait(int event, int param);
extern void arim_proto_arq_conn_out_wait(int event, int param);
extern void arim_proto_arq_conn_out_wait_rpt(int event, int param);
extern void arim_proto_arq_conn_pp_wait(int event, int param);
extern void arim_proto_arq_conn_in_wait(int event, int param);
extern void arim_proto_arq_conn_connected(int event, int param);

#endif
