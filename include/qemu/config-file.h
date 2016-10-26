#ifndef QEMU_CONFIG_H
#define QEMU_CONFIG_H

#include <stdio.h>
#include "qemu/option.h"
#include "qapi/error.h"
#include "qapi/qmp/qdict.h"

QemuOptsList *vmx_find_opts(const char *group);
QemuOptsList *vmx_find_opts_err(const char *group, Error **errp);
QemuOpts *vmx_find_opts_singleton(const char *group);

void vmx_add_opts(QemuOptsList *list);
void vmx_add_drive_opts(QemuOptsList *list);
int vmx_set_option(const char *str);
int vmx_global_option(const char *str);

void vmx_config_write(FILE *fp);
int vmx_config_parse(FILE *fp, QemuOptsList **lists, const char *fname);

int vmx_read_config_file(const char *filename);

/* Parse QDict options as a replacement for a config file (allowing multiple
   enumerated (0..(n-1)) configuration "sections") */
void vmx_config_parse_qdict(QDict *options, QemuOptsList **lists,
                             Error **errp);

/* Read default QEMU config files
 */
int vmx_read_default_config_files(bool userconfig);

#endif /* QEMU_CONFIG_H */
