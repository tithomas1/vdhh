/*
 * Copyright (C) 2016 Veertu Inc,
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __vmx__utils__
#define __vmx__utils__

#include <stdio.h>

void set_current_conf_name(char *new_conf_name);

ssize_t readLine(int fd, void *buffer, size_t n);

void generate_macaddr(uint8_t *macaddr);
char *macaddr_to_string(uint8_t *macaddr, char *buf);

void get_platform_uuid(char *buf, int bufSize);

CFMutableArrayRef get_cdroms();

bool osx_is_sierra();

#endif /* defined(__vmx__utils__) */
