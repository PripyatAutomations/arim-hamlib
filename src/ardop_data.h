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

#ifndef _ARDOP_DATA_H_INCLUDED_
#define _ARDOP_DATA_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif

extern int arim_data_waiting;
extern time_t arim_start_time;

extern void *ardop_data_func(void *data);
extern void ardop_data_inc_num_bytes_in(size_t num);
extern void ardop_data_inc_num_bytes_out(size_t num);
extern size_t ardop_data_get_num_bytes_in(void);
extern size_t ardop_data_get_num_bytes_out(void);
extern void ardop_data_reset_num_bytes(void);
extern size_t ardop_data_handle_data(unsigned char *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif

