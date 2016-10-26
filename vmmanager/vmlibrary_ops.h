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


#ifndef __VMLIBRARY_OPS_H__
#define __VMLIBRARY_OPS_H__

#include <pwd.h>

bool get_launch_param_string(char *vm_path, bool restore, char *buf, int buf_len);

typedef struct VMAddOnsSettings
{
    bool remap_cmd;
    bool enable_fs;
    bool enable_copy_paste;
    char fs_folder[PATH_MAX];
    bool hdpi;
} VMAddOnsSettings;

bool get_addons_settings(const char *vm_name, struct VMAddOnsSettings *settings);

uint64_t get_fs_free_space(char *folder);
uint64_t get_fs_size(char *folder);
void get_platform_uuid(char *buf, int bufSize);

#endif // __VMLIBRARY_OPS_H__
