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

#ifndef __COCOA_UTIL_H__
#define __COCOA_UTIL_H__

//#define COCA_DEBUG

#ifdef COCA_DEBUG
#define COCOA_DEBUG(...)  { (void) fprintf (stdout, __VA_ARGS__); }
#else
#define COCOA_DEBUG(...)  ((void) 0)
#endif

int cocoa_keycode_to_qemu(int keycode);

void suspend_vm();
void suspend_vm_internal();
void reboot_vm(bool forced);
void reset_vm();
void shutdown_vm(bool forced);

const char *get_vm_folder();

void get_screen_relosution(int *w, int *h);

void add_cdrom();
void unplug_cdrom();

void displayAlert(char *title, char *msg, char *button);

#endif
