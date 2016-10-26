#include <dirent.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include "hw.h"
#include "isa.h"
#include "ipc.h"
#include "veertuemu.h"
#include "nqdev.h"
#include "ui/console.h"
#include "ps2.h"
#include "vmlibrary_ops.h"
#include "cocoa_util.h"
//#include "vmm/vmx.h"

// TODO - REFACTOR: We courrently define these constants but use hard-coded numbers in code...
#define VMX_DEBUG_PORT  0x1850
#define VMX_HELPER_PORT 0x1854
#define VMX_MOUSE_PORT  0x1858
#define VMX_FS_PORT     0x185c

#define VMXPORT_ENTRIES 0x3

#ifdef DEBUG_PORT
#define DPRINTF(fmt, ...) \
do { printf(fmt, ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do { } while (0)
#endif

// TODO - REFACTOR: Should rename to "mouseEntry" to avoid confusion
QEMUPutMouseEntry *entry;

typedef struct MouseStat {
    uint32_t x;
    uint32_t y;
    uint32_t z;
    uint32_t button;
} MouseStat;

typedef struct VmxPortState
{
    ISADevice parent;
    
    VeertuMemArea io;
    
    uint8_t abs_mouse;
    uint8_t mouse_activated;

    uint32_t mouse_grabbed_need_sync;
    uint32_t mouse_ungrabbed_need_sync;
    
    uint32_t change_res;
    uint32_t change_res_x;
    uint32_t change_res_y;
    
    MouseStat mouse_stat;
    uint8_t helper_state;
    uint64_t helper_gva;
    uint32_t helper_ret_stat;
    
    uint8_t fs_helper_state;
    uint64_t fs_helper_gva;
    uint32_t fs_helper_ret_stat;
    
    char *copy_paste;
    char *copy_paste_from_guest;
    
    uint32_t last_button;
    uint32_t last_button_read;
    uint32_t ack;
    
    uint32_t mouse_version_read_write;
    uint32_t agent_version_read_write;
    uint32_t fs_version_read_write;
    
    //VmxPortReadFunc *func[VMXPORT_ENTRIES];
    void *opaque[VMXPORT_ENTRIES];
} VmxPortState;

VmxPortState *tmp_state;

extern ISADevice *i8042;
VmxPortState *vmx_port;

// TODO - BUG: need a lock
void vmx_grab_mouse(char *str)
{
    // TODO - BUG: This is probably not the solution but it will fix null-deref for now (when called from [VmView grabMouse])
    if (NULL != vmx_port) {
        vmx_port->mouse_grabbed_need_sync = 1;
        if (str) {
            if (vmx_port->copy_paste)
                free(vmx_port->copy_paste);
            vmx_port->copy_paste = malloc(strlen(str) + 1);
            VM_PANIC_ON(!vmx_port->copy_paste);
            strcpy(vmx_port->copy_paste, str);
        }
    }
}

char *get_copy_paste()
{
    return vmx_port->copy_paste;
}

void vmx_ungrab_mouse()
{
    vmx_port->mouse_ungrabbed_need_sync = 1;
}

void vmx_set_res(int x, int y)
{
    vmx_port->change_res = 1;
    vmx_port->change_res_x = x;
    vmx_port->change_res_y = y;
    DPRINTF("set res to %d %d\n", x, y);
}

void macvm_debug_port(uint32_t data, int size)
{
    switch (size) {
        case 1:
            DPRINTF("MACVM DEBUG %d\n", (uint8_t)data);
            break;
        case 2:
            DPRINTF("MACVM DEBUG %d\n", (uint16_t)data);
            break;
        case 4:
            DPRINTF("MACVM DEBUG %d\n", data);
            break;
    }
}

void mouse_handler_event(void *opaque, int x, int y, int dz, int buttons_state)
{
    VmxPortState *s = (VmxPortState *)opaque;
    int buttons = 0;
    
    if (!s->ack) {
        if (buttons_state & MOUSE_EVENT_LBUTTON || buttons_state & MOUSE_EVENT_RBUTTON) {
            i8042_isa_mouse_fake_event(i8042);
            return;
        }
        if (s->mouse_stat.button & (1 << 3)|| s->mouse_stat.button & (1 << 4) || s->mouse_stat.z) {
            i8042_isa_mouse_fake_event(i8042);
            return;
        }
    }
    
    if ((buttons_state & MOUSE_EVENT_LBUTTON)) {
        if (!(s->last_button & MOUSE_EVENT_LBUTTON))
            buttons |= 1 << 0;
    } else {
        if (s->last_button & MOUSE_EVENT_LBUTTON) {
            buttons |= 1 << 3;
        }
    }
    if ((buttons_state & MOUSE_EVENT_RBUTTON)) {
        if (!(s->last_button & MOUSE_EVENT_RBUTTON))
            buttons |= 1 << 1;
    } else {
        if (s->last_button & MOUSE_EVENT_RBUTTON) {
            buttons |= 1 << 4;
        }
    }
    if ((buttons_state & MOUSE_EVENT_MBUTTON)) {
        if (!(s->last_button & MOUSE_EVENT_MBUTTON))
            buttons |= 1 << 2;
    }
    
    s->last_button = buttons_state;
    x <<= 1;
    y <<= 1;
    s->mouse_stat.x = x;
    s->mouse_stat.y = y;
    s->mouse_stat.z = -dz;

    s->mouse_stat.button = buttons;
    if (buttons || s->mouse_stat.z) {
        s->ack = 0;
    } else {
        s->ack = 1;
    }

    i8042_isa_mouse_fake_event(i8042);
}

void update_mouse_handler(VmxPortState *s, int abs_mouse)
{
    if (entry) {
        vmx_remove_mouse_event_handler(entry);
        entry = NULL;
    }
    
    entry = vmx_add_mouse_event_handler(mouse_handler_event, s, abs_mouse, "mouse helper");
    vmx_activate_mouse_event_handler(entry);
    s->abs_mouse = abs_mouse;
    s->mouse_activated = 1;
}

static void inline write_to_gpa_index(struct CPUState *cpu, uint64_t gpa, uint32_t val, int index)
{
    address_space_rw(&address_space_memory, gpa + index * 4, &val, 4, 1);
}

static uint32_t inline read_from_gpa_index(struct CPUState *cpu, uint64_t gpa, int index)
{
    uint32_t val;
    
    address_space_rw(&address_space_memory, gpa + index * 4, &val, 4, 0);
    return val;
}

static uint32_t inline VM_MIN(uint32_t a, uint32_t b)
{
    if (a > b)
        return b;
    return a;
}

uint32_t need_to_copy = 0;
uint32_t need_to_copy_index = 0;

static int copy_to_guest_first(uint64_t gpa, uint8_t *buffer, int size)
{
    address_space_rw(&address_space_memory, gpa + 1024, buffer,  VM_MIN(size, 4096 - 1024), 1);
    need_to_copy = size - VM_MIN(size, 4096 - 1024);
    need_to_copy_index = 4096 - 1024;
    
    return need_to_copy;
}

static int copy_to_guest_second(uint64_t gpa, uint8_t *buffer)
{
    address_space_rw(&address_space_memory, gpa, buffer + need_to_copy_index,  VM_MIN(need_to_copy, 4096), 1);
    need_to_copy_index += VM_MIN(need_to_copy, 4096);
    need_to_copy -= VM_MIN(need_to_copy, 4096);
    
    return need_to_copy;
}

static int copy_from_guest_first(VmxPortState *s, uint64_t gpa, uint8_t *buffer, int size)
{
    need_to_copy = size;
    address_space_rw(&address_space_memory, gpa + 1024, (uint8_t *)s->copy_paste_from_guest,
                     VM_MIN(need_to_copy, 4096 - 1024), 0);
    need_to_copy_index = VM_MIN(need_to_copy, 4096 - 1024);
    need_to_copy -= VM_MIN(need_to_copy, 4096 - 1024);
    
    return need_to_copy;
}

static int copy_from_guest_second(uint64_t gpa, uint8_t *buffer)
{
    address_space_rw(&address_space_memory, gpa, buffer + need_to_copy_index,  VM_MIN(need_to_copy, 4096), 0);
    need_to_copy_index += VM_MIN(need_to_copy, 4096);
    need_to_copy -= VM_MIN(need_to_copy, 4096);
    
    return need_to_copy;
}
int get_res(int max_x, int max_y, int *x, int *y);

void alert_old_tools_ver();

static void vmx_guest_helper(VmxPortState *s, struct CPUState *cpu, uint32_t data, int size)
{
    uint32_t val;
    int agent_enabled  = 1;
    struct VMAddOnsSettings settings;
    
    if (get_addons_settings(get_vm_folder(), &settings))
        agent_enabled = settings.enable_copy_paste;

    switch (size) {
        case 1:
            val = (uint8_t)data;
            break;
        case 2:
            val = (uint16_t)data;
            break;
        case 4:
            val = data;
            break;
    }
    
    if (s->helper_state) {
        if (s->helper_state == 1) {
            s->helper_state++;
            s->helper_gva = (uint64_t)val;
        } else {
            s->helper_state = 0;
            s->helper_gva |= (((uint64_t)val) << 32);
        }
        DPRINTF("helper gva is %lx\n", s->helper_gva);
        s->helper_ret_stat = 0;
        return;
    }
    
    if (s->agent_version_read_write) {
        DPRINTF("agent version is %d\n", val);
        s->agent_version_read_write = 0;
        s->helper_ret_stat = 1; //our current agent version interface version
    
        if (val < 3) //we have older agent tools
            alert_old_tools_ver();
             
        return;
    }

    switch (val) {
        case 0x0: //set / get version
            s->agent_version_read_write = 1;
            break;
        case 0x2:
            s->helper_state = 1;
            s->helper_ret_stat = 0;
            break;
        case 0x4: {
            uint64_t gpa;
            
            //DPRINTF("need t copy %d\n",need_to_copy);
            if (!mmu_gva_to_gpa(cpu, s->helper_gva, &gpa)) {
                s->helper_ret_stat = 1;
            } else {
                if (agent_enabled) {
                    write_to_gpa_index(cpu, gpa, s->mouse_grabbed_need_sync && s->copy_paste, 1);
                    write_to_gpa_index(cpu, gpa, s->mouse_ungrabbed_need_sync, 2);
                    if (s->mouse_grabbed_need_sync && s->copy_paste) {
                        write_to_gpa_index(cpu, gpa, strlen(s->copy_paste) + 1, 3);
                        if (!copy_to_guest_first(gpa, (uint8_t *)s->copy_paste, strlen(s->copy_paste) + 1))
                            s->mouse_grabbed_need_sync = 0;
                    }
                }
                
                write_to_gpa_index(cpu, gpa, s->change_res, 4);
                if (s->change_res) {
                    write_to_gpa_index(cpu, gpa, s->change_res_x, 5);
                    write_to_gpa_index(cpu, gpa, s->change_res_y, 6);
                    s->change_res = 0;
                    DPRINTF("change_res\n");
                }
                
                s->helper_ret_stat = 0;
            }
            break;
        }
        case 0x5: {
            uint64_t gpa;
            DPRINTF("need t copy2 %d\n",need_to_copy);
            if (!mmu_gva_to_gpa(cpu, s->helper_gva, &gpa)) {
                s->helper_ret_stat = 1;
            } else {
                if (agent_enabled) {
                    if (!copy_to_guest_second(gpa, (uint8_t *)s->copy_paste))
                        s->mouse_grabbed_need_sync = 0;
                }
                s->helper_ret_stat = 0;
            }
            break;
        }
        case 0x6: {
            uint64_t gpa;
            
            if (!mmu_gva_to_gpa(cpu, s->helper_gva, &gpa)) {
                s->helper_ret_stat = 1;
            } else {
                if (agent_enabled) {
                    int __need_to_copy = read_from_gpa_index(cpu, gpa, 0);
                    VM_PANIC_ON(__need_to_copy > 1024 * 1024 * 128);
                    if (s->copy_paste_from_guest)
                        free(s->copy_paste_from_guest);
                    s->copy_paste_from_guest = malloc(__need_to_copy);
                    VM_PANIC_ON(!s->copy_paste_from_guest);
                    if (!copy_from_guest_first(s, gpa, (uint8_t *)s->copy_paste_from_guest, __need_to_copy)) {
                        s->mouse_ungrabbed_need_sync = 0;
                        s->copy_paste_from_guest[need_to_copy_index - 1] = NULL;
                        cocoa_set_clipboard(s->copy_paste_from_guest);
                        free(s->copy_paste_from_guest);
                        s->copy_paste_from_guest = NULL;
                    }
                }
                s->helper_ret_stat = 0;
            }
            break;
        }
        case 0x7: {
            uint64_t gpa;
            
            if (!mmu_gva_to_gpa(cpu, s->helper_gva, &gpa)) {
                s->helper_ret_stat = 1;
            } else {
                if (agent_enabled) {
                    VM_PANIC_ON(!s->copy_paste_from_guest);
                    if (!copy_from_guest_second(gpa, (uint8_t *)s->copy_paste_from_guest)) {
                        s->mouse_ungrabbed_need_sync = 0;
                        s->copy_paste_from_guest[need_to_copy_index - 1] = '\0';
                        cocoa_set_clipboard(s->copy_paste_from_guest);
                        free(s->copy_paste_from_guest);
                        s->copy_paste_from_guest = NULL;
                    }
                }
                s->helper_ret_stat = 0;
            }
            break;
        }
        case 0x8: {
            uint64_t gpa;

            if (!mmu_gva_to_gpa(cpu, s->helper_gva, &gpa)) {
                s->helper_ret_stat = 1;
            } else {
                int max_w, max_h;
                int x, y;
                get_screen_relosution(&max_w, &max_h);
                if (get_res(max_w, max_h, &x, &y)) {
                    write_to_gpa_index(cpu, gpa, x, 2);
                    write_to_gpa_index(cpu, gpa, y, 3);
                    write_to_gpa_index(cpu, gpa, 1, 1);
                } else {
                    write_to_gpa_index(cpu, gpa, 0, 1);
                }
                s->helper_ret_stat = 0;
            }
            break;
        }
            
        case 0x9: {
            uint64_t gpa;
            if (!mmu_gva_to_gpa(cpu, s->helper_gva, &gpa)) {
                s->helper_ret_stat = 1;
            } else {
                time_t t = time(NULL);
                struct tm tm = *localtime(&t);
                write_to_gpa_index(cpu, gpa, 1, 1);
                write_to_gpa_index(cpu, gpa, tm.tm_year + 1900, 2);
                write_to_gpa_index(cpu, gpa, tm.tm_mon + 1, 3);
                write_to_gpa_index(cpu, gpa, tm.tm_mday,  4);
                write_to_gpa_index(cpu, gpa, tm.tm_hour,  5);
                write_to_gpa_index(cpu, gpa, tm.tm_min,  6);
                write_to_gpa_index(cpu, gpa, tm.tm_sec,  7);
                s->helper_ret_stat = 0;
            }
            break;
            
        }
        default:
            //VM_PANIC("no index agent");
                        break;
    }
}


static uint32_t vmx_guest_helper_ret_stat(VmxPortState *s, struct CPUState *cpu, int size)
{
    switch (size) {
        case 1:
            return (uint8_t)s->helper_ret_stat;
        case 2:
            return (uint16_t)s->helper_ret_stat;
        case 4:
            return (uint32_t)s->helper_ret_stat;
    }
    return 0;
}

static void vmx_guest_helper_mouse_write(VmxPortState *s, struct CPUState *cpu, uint32_t data, int size)
{
    uint32_t val;
    
    switch (size) {
        case 1:
            val = (uint8_t)data;
            break;
        case 2:
            val = (uint16_t)data;
            break;
        case 4:
            val = (uint32_t)data;
            break;
    }
    
    if (s->mouse_version_read_write) {
        DPRINTF("mouse driver version %d\n", val);
        return;
    }
    
    switch (val) {
        case 0: //mouse driver version
            s->mouse_version_read_write = 1;
            break;
        case 0x1:
            DPRINTF("set mouse\n");
            update_mouse_handler(s, 1);
            break;
        default:
            //VM_PANIC("no index mouse");
                        break;
    }
}

static int mouse_place = 1;

static uint32_t vmx_guest_helper_mouse_read(VmxPortState *s, struct CPUState *cpu, int size)
{
    uint32_t val;
    
    if (s->mouse_version_read_write) { //return mouse version
        s->mouse_version_read_write = 0;
        return 1;
    }
    
    switch (mouse_place) {
        case 0x1:
            val = s->mouse_stat.x;
            mouse_place++;
            break;
        case 0x2:
            val = s->mouse_stat.y;
            mouse_place++;
            break;
        case 0x3:
            val = s->mouse_stat.button;
            mouse_place++;
            if (val) {
                s->ack = 1;
            }
            break;
        case 0x4:
            val = s->mouse_stat.z;
            if (val) {
                s->ack = 1;
            }
            mouse_place = 1;
            break;
        default:
            //VM_PANIC("no index mouse2");
                        break;
    }
    
    switch (size) {
        case 1:
            return (uint8_t)val;
        case 2:
            return (uint16_t)val;
        case 4:
            return val;
    }
    return 0;
}

void timespec_to_timeval(struct timespec *ts, struct timeval *tv)
{
    tv->tv_sec  = ts->tv_sec;
    tv->tv_usec = ts->tv_nsec / 1000;
}

#define EPOCH_DIFF 11644473600LL

unsigned long long getfiletime(struct timespec *ts) {
    struct timeval tv;
    unsigned long long result = EPOCH_DIFF;

    timespec_to_timeval(ts, &tv);

    result += tv.tv_sec;
    result *= 10000000LL;
    result += tv.tv_usec * 10;
    return result;
}

int file_fd = -1;

DIR *dir = NULL;
char dir_name[4096];
int dir_mode = 0;

#define MAX_FILE_HANDLERS 100

typedef struct file_handler {
    int used;
    char name[4096];
    int is_dir;
    DIR *cur_dir;
    int file_fd;
    uint32_t id;
} file_handler;

file_handler file_handlers[MAX_FILE_HANDLERS];


void init_file_handlers()
{
    int x;
    
    for (x = 0; x < MAX_FILE_HANDLERS; ++x) {
        file_handlers[x].used = 0;
    }
}

file_handler *get_file_handler(uint32_t id)
{
    if (id >= MAX_FILE_HANDLERS)
        return NULL;

    if (!file_handlers[id].used)
        return NULL;

    return &file_handlers[id];
}

file_handler *get_free_file_handler()
{
    int x;

    for (x = 0; x < MAX_FILE_HANDLERS; ++x) {
        if (!file_handlers[x].used) {
            file_handlers[x].id = x;
            file_handlers[x].file_fd = -1;
            file_handlers[x].cur_dir = NULL;
            file_handlers[x].is_dir = 0;
            file_handlers[x].used = 1;
            return &file_handlers[x];
        }
    }

    return NULL;
}

void set_fixed_filename(char *fixed_name, char * name)
{
    fixed_name[0] = 0;
    struct VMAddOnsSettings settings;
    if (get_addons_settings(get_vm_folder(), &settings))
        snprintf(fixed_name, 4096, "%s/%s", settings.fs_folder, name);
}

int recursiveDelete(char* dirname) {
    
    DIR *dp;
    struct dirent *ep;
    
    char abs_filename[4096];
    
    dp = opendir (dirname);
    if (dp != NULL)
    {
        while (ep = readdir (dp)) {
            struct stat stFileInfo;
            
            snprintf(abs_filename, 4096, "%s/%s", dirname, ep->d_name);
            abs_filename[4095] = '\0';
            
            if (lstat(abs_filename, &stFileInfo) < 0) //error
                return 0;
            
            if(S_ISDIR(stFileInfo.st_mode)) {
                if(strcmp(ep->d_name, ".") &&
                   strcmp(ep->d_name, "..")) {
                   recursiveDelete(abs_filename);
                }
            } else {
                remove(abs_filename);
            }
        }
        closedir (dp);
    }
    else
        perror ("Couldn't open the directory");
    
    
    remove(dirname);
    return 0;
    
}


void free_file_handler(uint32_t id, uint32_t rm)
{
    char fixed_name[4096];
    file_handler *file_handler;

    file_handler = get_file_handler(id);
    if (!file_handler) {
        DPRINTF("cant find file handler to release !\n");
        return;
    }
    
    if (file_handler->cur_dir) {
        closedir(file_handler->cur_dir);
        if (rm) {
            set_fixed_filename(fixed_name, file_handler->name);
            fixed_name[4095] = '\0';
            DPRINTF("rmdir %s\n", fixed_name);
            recursiveDelete(fixed_name);
        }
    }
    if (file_handler->file_fd != -1) {
        close(file_handler->file_fd);
        if (rm) {
            set_fixed_filename(fixed_name, file_handler->name);
            fixed_name[4095] = '\0';
            DPRINTF("unlink %s\n", fixed_name);
            unlink(fixed_name);
        }
    }

    file_handler->cur_dir = NULL;
    file_handler->file_fd = -1;
    file_handler->used = 0;
    file_handler->is_dir = 0;
    file_handler->used = 0;
}

void reset_file_handlers()
{
    int x;

    DPRINTF("reset_file_handlers\n");

    for (x = 0; x < MAX_FILE_HANDLERS; ++x) {
        if (file_handlers[x].used) {
            free_file_handler(x, 0);
        }
    }
}

int set_dir()
{
    if (dir) {
        closedir(dir);
        dir = NULL;
    }
    dir = opendir(dir_name);

    //DPRINTF("set dir name: %s\n", dir_name);
    if (!dir) {
        //DPRINTF("here0\n");
        return 0;
    }
    
    return 1;
}

int read_stat(char * buffer, int *is_dir, int *is_link, uint64_t *last_acc,
              uint64_t *last_mod, uint64_t *last_stat, uint64_t *creation_time,
            uint64_t *file_size)
{
    int status;
    struct stat st_buf;

    *is_dir = 0;
    *is_link = 0;

    status = stat(buffer, &st_buf);
    if (!status) {
        if (S_ISDIR(st_buf.st_mode)) {
            *is_dir = 1;
        }
        if (S_ISLNK(st_buf.st_mode)) {
            *is_link = 1;
        }
        *last_acc = getfiletime(&st_buf.st_atimespec);
        *last_mod = getfiletime(&st_buf.st_mtimespec);
        *last_stat = getfiletime(&st_buf.st_ctimespec);
        *creation_time = getfiletime(&st_buf.st_birthtimespec);
        *file_size = st_buf.st_size;
        return 1;
    } else {
        //DPRINTF("annanana\n");
        return 0;
    }
}

int read_dir_ent(file_handler *file_handler, char *buffer, int *is_dir, int *is_link, uint64_t *last_acc,
                    uint64_t *last_mod, uint64_t *last_stat, uint64_t *creation_time,
                   uint64_t *file_size)
{
    struct dirent *ent;
    char fixed_name[4096];
    char statname[4096];

    ent = readdir(file_handler->cur_dir);
    if (!ent) {
        //DPRINTF("here2\n");
        return 0;
    }

    set_fixed_filename(fixed_name, file_handler->name);
    snprintf(statname, 4096, "%s/%s", fixed_name, ent->d_name);
    statname[4095] = '\0';
    DPRINTF("statname is %s\n", statname);
    read_stat(statname, is_dir, is_link, last_acc, last_mod, last_stat, creation_time, file_size);

    strncpy(buffer, ent->d_name, 1024);
    buffer[1023] = '\0';
    
    return 1;
}

void make_unix_string(char *str)
{
    int x = 0;
    while(str[x] != '\0') {
    if (str[x] == '\\')
        str[x] = '/';
        ++x;
    }
    DPRINTF("x is %d\n", x);
}

uint64_t DirSize(char* dirname)
{
    return get_fs_size(dirname);
}

uint64_t userAvailableFreeSpace()
{
    struct statvfs stat;
    char fixed_filename[4096];
    
    set_fixed_filename(fixed_filename, "");
    statvfs(fixed_filename, &stat);
    {
        uint64_t freeBytes = (uint64_t)stat.f_bavail * stat.f_frsize;
        return freeBytes;
    }
    return 0ULL;
}

static void vmx_fs_helper(VmxPortState *s, struct CPUState *cpu, uint32_t data, int size)
{
    uint32_t val;
    int fs_enabled  = 1;
    struct VMAddOnsSettings settings;
    
    if (get_addons_settings(get_vm_folder(), &settings))
        fs_enabled = settings.enable_fs;

    switch (size) {
        case 1:
            val = ((uint8_t)data);
            break;
        case 2:
            val = ((uint16_t)data);
            break;
        case 4:
            val = ((uint32_t)data);
            break;
    }

    DPRINTF("this is %x %d\n", data, size);

    if (s->fs_helper_state) {
        if (s->fs_helper_state == 1) {
            s->fs_helper_state++;
            s->fs_helper_gva = (uint64_t)val;
        } else {
            s->fs_helper_state = 0;
            s->fs_helper_gva |= (((uint64_t)val) << 32);
        }
        DPRINTF("helper gva is %lx\n", s->fs_helper_gva);
        s->fs_helper_ret_stat = 0;
        return;
    }
    
    if (s->fs_version_read_write) {
        s->fs_version_read_write = 0;
        DPRINTF("fs drver version is %d\n", val);
        s->fs_helper_ret_stat = 1; //our current fs interface version
        return;
    }

    DPRINTF("val is %d\n", val);
    switch (val) {
        case 0x0: //version
            s->fs_version_read_write = 1;
            break;
        case 0x2:
            s->fs_helper_state = 1;
            s->fs_helper_ret_stat = 0;
            init_file_handlers();
            break;
        case 0x3: { //debug print
            uint64_t gpa;

            if (!mmu_gva_to_gpa(cpu, s->fs_helper_gva, &gpa)) {
                s->fs_helper_ret_stat = 1;
            } else {
                char str[4096];

                address_space_rw(&address_space_memory, gpa, str,  4096 - 1024, 0);
                str[4095 - 1024] = '\0';
                DPRINTF("PRINT: %s\n", str);
                s->fs_helper_ret_stat = 0;
            }
            break;
        }
        case 0x4: { //open file / dir
            uint64_t gpa;
 
            if (!mmu_gva_to_gpa(cpu, s->fs_helper_gva, &gpa)) {
                s->fs_helper_ret_stat = 1;
            } else {
                uint32_t a_create;
                uint32_t a_open_if;
                uint32_t a_open;
                uint32_t a_overwrite;
                uint32_t a_overwrite_if;
                uint32_t a_replace;
                uint32_t a_dir;
                uint32_t write_acc;

                int is_dir = 0;
                int is_link = 0;
                int exist = 0;
                uint32_t id;
                uint64_t last_acc = 0;
                uint64_t last_mod = 0;
                uint64_t last_stat = 0;
                uint64_t creation_time = 0;
                uint64_t file_size = 0;

                char fixed_filename[4096];
                int oflags = 0;
                file_handler *file_handler;
 
                s->fs_helper_ret_stat = 0;
                
                if (!fs_enabled)
                    break;

                file_handler = get_free_file_handler();
                if (!file_handler) {
                    write_to_gpa_index(cpu, gpa, 0, 0);
                    break;
                }

            address_space_rw(&address_space_memory, gpa, file_handler->name,  4096 - 1024, 0);
            file_handler->name[4095 - 1024] = '\0';
            make_unix_string(file_handler->name);
            set_fixed_filename(fixed_filename, file_handler->name);
            exist = read_stat(fixed_filename, &is_dir, &is_link, &last_acc, &last_mod, &last_stat, &creation_time, &file_size);

            a_create = read_from_gpa_index(cpu, gpa, (1024 / 4) * 3);
            a_open_if = read_from_gpa_index(cpu, gpa, (1024 / 4) * 3 + 1);
            a_open = read_from_gpa_index(cpu, gpa, (1024 / 4) * 3 + 2);
            a_overwrite = read_from_gpa_index(cpu, gpa, (1024 / 4) * 3 + 3);
            a_overwrite_if = read_from_gpa_index(cpu, gpa, (1024 / 4) * 3 + 4);
            a_replace = read_from_gpa_index(cpu, gpa, (1024 / 4) * 3 + 5);
            a_dir = read_from_gpa_index(cpu, gpa, (1024 / 4) * 3 + 6);

            write_acc = read_from_gpa_index(cpu, gpa, (1024 / 4) * 3 + 7);

            DPRINTF("create file %s with create %d open_if %d open %d overwrite_if %d overrite %d replace %d dir %d\n", fixed_filename, a_create, a_open_if, a_open, a_overwrite_if, a_overwrite, a_replace, a_dir);
            DPRINTF("openinfo %s is dir %d is link %d\n", fixed_filename, is_dir, is_link);

            if (!exist) {
                is_dir = a_dir;
            }
                
            if (file_handler->name[0] == '\0')
                is_dir = a_dir = 1;

            if (!is_dir) {
                if (a_create) {
                    oflags |= (O_EXCL | O_CREAT);
                }

                if (a_open_if) {
                    oflags |= O_CREAT;
                }

                if (a_overwrite) {
                    oflags |= O_TRUNC;
                }
   
                if (a_overwrite_if)
                    oflags |= O_TRUNC | O_CREAT;

                if (a_replace) //not completely true?
                    oflags |= O_TRUNC | O_CREAT;

                if (write_acc)
                    oflags |= O_RDWR;
                else
                    oflags |= O_RDONLY;
 
                file_handler->file_fd = open(fixed_filename, oflags, 0777);
                if (file_handler->file_fd == -1) {
                    write_to_gpa_index(cpu, gpa, 0, 0);
                    free_file_handler(file_handler->id, 0);
                    DPRINTF("failed to open a file\n");
                } else {
                    write_to_gpa_index(cpu, gpa, 1, 0);
                    write_to_gpa_index(cpu, gpa, file_handler->id, 1);
                    file_handler->is_dir = 0;
                }
           } else {
               if (a_create || a_open_if) {
                   mkdir(fixed_filename, 0777);
               }
               file_handler->cur_dir = opendir(fixed_filename);
               if (!file_handler->cur_dir) {
                   write_to_gpa_index(cpu, gpa, 0, 0);
                   free_file_handler(file_handler->id, 0);
                   DPRINTF("failed to create dir\n");
                   perror("opendir");
               } else {
                   write_to_gpa_index(cpu, gpa, 1, 0);
                   write_to_gpa_index(cpu, gpa, file_handler->id, 1);
                   file_handler->is_dir = 1;
               }
           }
out_4:
           DPRINTF("create with id %d\n",file_handler->id);
           s->fs_helper_ret_stat = 0;
           }
           break;
        }
        case 0x5: { //dir query info
            uint64_t gpa;
  
            if (!mmu_gva_to_gpa(cpu, s->fs_helper_gva, &gpa)) {
                s->helper_ret_stat = 1;
            } else {
                int is_dir;
                int is_link;
                int ret;
                uint32_t id;
                uint64_t last_acc;
                uint64_t last_mod;
                uint32_t reset;
                uint64_t last_stat;
                uint64_t creation_time;
                uint64_t file_size;
                char filename[1024];
                file_handler *file_handler;

                s->fs_helper_ret_stat = 0;
                
                if (!fs_enabled)
                    break;

                DPRINTF("start process\n");
                id = read_from_gpa_index(cpu, gpa, (1024 / 4) * 3);
                file_handler = get_file_handler(id);

                if (!file_handler) {
                    DPRINTF("didnt find file handler for %d\n", id);
                    write_to_gpa_index(cpu, gpa, 0, 0); //no such handler
                    break;
                }

                if (!file_handler->is_dir) {
                    DPRINTF("not a dir %d\n", id);
                    write_to_gpa_index(cpu, gpa, 0, 0); //no such handler
                    break;
                }

                DPRINTF("mid process\n");

                write_to_gpa_index(cpu, gpa, 1, 0); //found handler

                reset = read_from_gpa_index(cpu, gpa, (1024 / 4) * 3 + 1);
                if (reset)
                    rewinddir(file_handler->cur_dir);

                    ret = read_dir_ent(file_handler, filename, &is_dir, &is_link, &last_acc, &last_mod,
                                       &last_stat, &creation_time, &file_size);
                    if (!ret) {
                        write_to_gpa_index(cpu, gpa, 0, 1); //no more files
                        DPRINTF("didnt find file !\n");
                        break;
                    }

                    DPRINTF("write %d file that is %s dir %d\n", id, filename, is_dir);
                    filename[4095 - 1024] = '\0';
                    address_space_rw(&address_space_memory, gpa + 1024, filename, 4096 - 1024, 1);

                    write_to_gpa_index(cpu, gpa, 1, 1); //found file

                    write_to_gpa_index(cpu, gpa, is_dir, 2); //dir file
 
                    if (filename[0] == '.')
                        write_to_gpa_index(cpu, gpa, 1, 3); //hiden file
                    else
                        write_to_gpa_index(cpu, gpa, 0, 3);

                    write_to_gpa_index(cpu, gpa, is_link, 4); //file is link

                    write_to_gpa_index(cpu, gpa, (uint32_t)last_acc, 5);
                    write_to_gpa_index(cpu, gpa, (uint32_t)(last_acc >> 32), 6);

                    write_to_gpa_index(cpu, gpa, (uint32_t)last_mod,7);
                    write_to_gpa_index(cpu, gpa, (uint32_t)(last_mod >> 32), 8);

                    write_to_gpa_index(cpu, gpa, (uint32_t)last_stat, 9);
                    write_to_gpa_index(cpu, gpa, (uint32_t)(last_stat >> 32), 10);
 
                    write_to_gpa_index(cpu, gpa, (uint32_t)creation_time, 11);
                    write_to_gpa_index(cpu, gpa, (uint32_t)(creation_time >> 32), 12);

                    write_to_gpa_index(cpu, gpa, (uint32_t)file_size, 13);
                    write_to_gpa_index(cpu, gpa, (uint32_t)(file_size >> 32), 14);
                
                    DPRINTF("end process\n");
                    //DPRINTF("file size is %d\n", file_size);
                }
                break;
            }
            case 0x6: { //files info
                uint64_t gpa;

                if (!mmu_gva_to_gpa(cpu, s->fs_helper_gva, &gpa)) {
                    s->fs_helper_ret_stat = 1;
                } else {
                    int is_dir = 0;
                    int is_link = 0;
                    uint32_t id;
                    uint64_t last_acc = 0;
                    uint64_t last_mod = 0;
                    uint64_t last_stat = 0;
                    uint64_t creation_time = 0;
                    uint64_t file_size = 0;
                    char statname[4096];
                    file_handler *file_handler;

                    s->fs_helper_ret_stat = 0;
                    
                    if (!fs_enabled)
                        break;

                    id = read_from_gpa_index(cpu, gpa, (1024 / 4) * 3);
                    file_handler = get_file_handler(id);

                    if (!file_handler) {
                        write_to_gpa_index(cpu, gpa, 0, 0); //no such handler
                        break;
                    }

                    write_to_gpa_index(cpu, gpa, 1, 0); //found handler

                    set_fixed_filename(statname, file_handler->name);

                    read_stat(statname, &is_dir, &is_link, &last_acc, &last_mod, &last_stat, &creation_time, &file_size);
                    DPRINTF("fileinfo %d %s is dir %d is link %d\n",id, statname, is_dir, is_link);

                    write_to_gpa_index(cpu, gpa, 1, 1);

                    write_to_gpa_index(cpu, gpa, is_dir, 2);
                    if (statname[0] == '.')
                        write_to_gpa_index(cpu, gpa, 1, 3); //hiden file
                    else
                        write_to_gpa_index(cpu, gpa, 0, 3);

                        write_to_gpa_index(cpu, gpa, is_link, 4);

                        write_to_gpa_index(cpu, gpa, (uint32_t)last_acc, 5);
                        write_to_gpa_index(cpu, gpa, (uint32_t)(last_acc >> 32), 6);

                        write_to_gpa_index(cpu, gpa, (uint32_t)last_mod, 7);
                        write_to_gpa_index(cpu, gpa, (uint32_t)(last_mod >> 32), 8);

                        write_to_gpa_index(cpu, gpa, (uint32_t)last_stat, 9);
                        write_to_gpa_index(cpu, gpa, (uint32_t)(last_stat >> 32), 10);

                        write_to_gpa_index(cpu, gpa, (uint32_t)creation_time, 11);
                        write_to_gpa_index(cpu, gpa, (uint32_t)(creation_time >> 32), 12);

                        write_to_gpa_index(cpu, gpa, (uint32_t)file_size, 13);
                        write_to_gpa_index(cpu, gpa, (uint32_t)(file_size >> 32), 14);
                    }
                    break;
                }
                case 0x7: { //close
                    uint64_t gpa;

                    if (!mmu_gva_to_gpa(cpu, s->fs_helper_gva, &gpa)) {
                        s->fs_helper_ret_stat = 1;
                    } else {
                        uint32_t id;
                        uint32_t rm;
                        file_handler *file_handler;

                        s->fs_helper_ret_stat = 0;
                        
                        if (!fs_enabled)
                            break;
                        
                        id = read_from_gpa_index(cpu, gpa, (1024 / 4) * 3);
                        rm = read_from_gpa_index(cpu, gpa, (1024 / 4) * 3 + 1);

                        free_file_handler(id, rm);
                    }
                    break;
                }
                case 0x8: { //rename
                    uint64_t gpa;

                    if (!mmu_gva_to_gpa(cpu, s->fs_helper_gva, &gpa)) {
                        s->fs_helper_ret_stat = 1;
                    } else {
                        uint32_t id;
                        uint32_t replace;
                        char orig_fixed[4096];
                        char new_fixed[4096];
                        char new_tmp[4096];
                        file_handler *file_handler;

                        s->fs_helper_ret_stat = 0;
                        
                        if (!fs_enabled)
                            break;

                        address_space_rw(&address_space_memory, gpa, new_tmp,  4096 - 1024, 0);

                        id = read_from_gpa_index(cpu, gpa, (1024 / 4) * 3);
                        file_handler = get_file_handler(id);

                        if (!file_handler) {
                            write_to_gpa_index(cpu, gpa, 0, 0); //no such handler
                            break;
                        }

                        write_to_gpa_index(cpu, gpa, 1, 0); //found handler

                        replace = read_from_gpa_index(cpu, gpa, (1024 / 4) * 3 + 1);

                        set_fixed_filename(orig_fixed, file_handler->name);

                        new_tmp[4095 - 1024] = '\0';
                        DPRINTF("1new tmp is %s\n", new_tmp);
                        make_unix_string(new_tmp);
                        DPRINTF("after unix string is %s\n", new_tmp);
                        set_fixed_filename(new_fixed, new_tmp);

                        if (!replace) {
                            if (!file_handler->is_dir) {
                                int file_fd;

                                file_fd = open(new_fixed, 0);
                                if (file_fd != -1) {
                                    write_to_gpa_index(cpu, gpa, 0, 1); //new file already exist !
                                    close(file_fd);
                                    DPRINTF("cant rename file exist\n");
                                } else {
                                    write_to_gpa_index(cpu, gpa, 1, 1);
                                }
                            } else {
                                DIR *dir;

                                dir = opendir(new_fixed);
                                if (dir) {
                                    write_to_gpa_index(cpu, gpa, 0, 1);
                                    closedir(dir);
                                    DPRINTF("cant renmae dir exist\n");
                                }   else {
                                    write_to_gpa_index(cpu, gpa, 1, 1);
                                }
                            }
                        }

                        DPRINTF("rename from %s to %s\n", orig_fixed, new_fixed);

                        if (rename(orig_fixed, new_fixed)) {
                            write_to_gpa_index(cpu, gpa, 0, 2);
                        } else {
                            write_to_gpa_index(cpu, gpa, 1, 2);
                            strcpy(file_handler->name, new_tmp);
                        }
                    }
                    break;
                }
                case 0x9: { //read \ write
                    uint64_t gpa;
                    
                    if (!mmu_gva_to_gpa(cpu, s->fs_helper_gva, &gpa)) {
                        s->fs_helper_ret_stat = 1;
                    } else {
                        int ret_size;
                        uint32_t id;
                        uint32_t page_offset;
                        uint32_t size;
                        uint32_t is_write;
                        uint64_t file_offset;
                        uint64_t read_gpa;
                        file_handler *file_handler;
                        char readbuffer[4096];

                        s->fs_helper_ret_stat = 0;
                        
                        if (!fs_enabled)
                            break;
                        
                        id = read_from_gpa_index(cpu, gpa, (1024 /4) * 3);
                        file_handler = get_file_handler(id);

                        if (!file_handler) {
                            write_to_gpa_index(cpu, gpa, 0, 0); //no such handler
                            break;
                        }

                        write_to_gpa_index(cpu, gpa, 1, 0);

                        read_gpa = (uint64_t)read_from_gpa_index(cpu, gpa, (1024 / 4) * 3 + 1);
                        read_gpa = read_gpa << 12;
                        page_offset = read_from_gpa_index(cpu, gpa, (1024 / 4) * 3 + 2);
                        read_gpa += page_offset;

                        file_offset = read_from_gpa_index(cpu, gpa, (1024 / 4) * 3 + 4);
                        file_offset = file_offset << 32;
                        file_offset |= read_from_gpa_index(cpu, gpa, (1024 / 4) * 3 + 3);

                        size = read_from_gpa_index(cpu, gpa, (1024 / 4) * 3 + 5);
                        if (size > 4096) {
                            DPRINTF("size error !! %d\n", size);
                            size = 4096;
                        }

                        is_write = read_from_gpa_index(cpu, gpa, (1024 / 4) * 3 + 6);

                        DPRINTF("hrmm %d read_gpa %lx page_offset %x file_offset %lx size %d %d\n", id, read_gpa, page_offset, file_offset, size, is_write);

                        lseek(file_handler->file_fd, file_offset, SEEK_SET);

                        if (!is_write) {
                            ret_size = read(file_handler->file_fd, readbuffer, size);
                            if (ret_size > 0) {
                                address_space_rw(&address_space_memory, read_gpa, readbuffer, ret_size, 1);
                                write_to_gpa_index(cpu, gpa, ret_size, 1);
                            } else {
                                write_to_gpa_index(cpu, gpa, 0, 1);
                            }
                        } else {
                            address_space_rw(&address_space_memory, read_gpa, readbuffer, size, 0);
                            ret_size = write(file_handler->file_fd, readbuffer, size);
                            if (ret_size > 0) {
                                write_to_gpa_index(cpu, gpa, ret_size, 1);
                            } else {
                                write_to_gpa_index(cpu, gpa, 0, 1);
                            }
                        }
                    }
                    break;
                }
                case 10: { //vol info
                    uint64_t gpa;
            
                    if (!mmu_gva_to_gpa(cpu, s->fs_helper_gva, &gpa)) {
                        s->fs_helper_ret_stat = 1;
                    } else {
                        uint64_t disk_size = 0;
                        uint64_t disk_space = 0;
                        char fixed_filename[4096];
                        
                        s->fs_helper_ret_stat = 0;
                        
                        if (!fs_enabled)
                            break;
                        
                        set_fixed_filename(fixed_filename, "");
                        
                        disk_size = DirSize(fixed_filename);
                        disk_space = userAvailableFreeSpace();
                        
                        write_to_gpa_index(cpu, gpa, 1, 0);
                        
                        write_to_gpa_index(cpu, gpa, (uint32_t)disk_size, 2);
                        write_to_gpa_index(cpu, gpa, (uint32_t)(disk_size >> 32), 3);
                
                        write_to_gpa_index(cpu, gpa, (uint32_t)disk_space, 4);
                        write_to_gpa_index(cpu, gpa, (uint32_t)(disk_space >> 32), 5);
                    }
                    break;
                }
                default:
                    //VM_PANIC("no index fs");
            break;
        }
}

static uint32_t vmx_fs_ret_stat(VmxPortState *s, struct CPUState *cpu, int size)
{
    uint32_t val;
    
    switch (size) {
        case 1:
            return (uint8_t)s->fs_helper_ret_stat;
        case 2:
            return (uint16_t)s->fs_helper_ret_stat;
        case 4:
            return (uint32_t)s->fs_helper_ret_stat;
    }
    
    return (uint32_t)s->fs_helper_ret_stat;
}

//#define VMPORT_DEBUG
//#define VMPORT_MAGIC   0x564D5868

#define TYPE_VMXPORT "vmx_port"
#define VMPORT(obj)  obj

DECLARE_TLS(CPUState *, current_cpu);
#define current_cpu tls_var(current_cpu)

static uint32_t vmx_port_ioport_read(void *opaque, hwaddr addr, unsigned size)
{
    VmxPortState *s = opaque;
    CPUState *cs = current_cpu;
    
    if (addr == 1)
        return vmx_guest_helper_ret_stat(s, cs, size);
    
    if (addr == 2) {
        uint32_t v = vmx_guest_helper_mouse_read(s, cs, size);
        return v;
    }
    
    if (addr == 3)
        return vmx_fs_ret_stat(s, cs, size);

    return 0;
}

static void vmx_port_ioport_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    VmxPortState *s = opaque;
    CPUState *cs = current_cpu;

    if (addr == 0) {
        macvm_debug_port(val, size);
        return;
    }
    
    if (addr == 1) {
        vmx_guest_helper(s, cs, val, size);
        return;
    }
    
    if (addr == 2) {
        vmx_guest_helper_mouse_write(s, cs, val, size);
        return;
    }
    
    if (addr == 3) {
        vmx_fs_helper(s, cs, val, size);
    }
}

void vmx_fs_port_write(hwaddr addr, uint64_t val, unsigned size)
{
    vmx_port_ioport_write(tmp_state, addr, val, size);
}

uint32_t vmx_fs_port_read(hwaddr addr, unsigned size)
{
    return vmx_port_ioport_read(tmp_state, addr, size);
}

static int vmx_post_load(void *opaque, int version_id)
{
    VmxPortState *s = (VmxPortState*)opaque;
    if (s->mouse_activated) {
        update_mouse_handler(s, s->abs_mouse);
    }
    return 0;
}


const VMStateDescription vmstate_vmx = {
    .name = "VMX State",
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = vmx_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(mouse_grabbed_need_sync, VmxPortState),
        VMSTATE_UINT32(mouse_ungrabbed_need_sync, VmxPortState),
        
        VMSTATE_UINT32(change_res, VmxPortState),
        VMSTATE_UINT32(change_res_x, VmxPortState),
        VMSTATE_UINT32(change_res_y, VmxPortState),
        
        
        VMSTATE_UINT8(mouse_activated, VmxPortState),
        VMSTATE_UINT8(abs_mouse, VmxPortState),
        VMSTATE_UINT32(mouse_stat.x, VmxPortState),
        VMSTATE_UINT32(mouse_stat.y, VmxPortState),
        VMSTATE_UINT32(mouse_stat.z, VmxPortState),
        VMSTATE_UINT32(mouse_stat.button, VmxPortState),
        VMSTATE_UINT32(last_button, VmxPortState),
        VMSTATE_UINT32(last_button_read, VmxPortState),
        VMSTATE_UINT32(ack, VmxPortState),
        
        VMSTATE_UINT32(mouse_version_read_write, VmxPortState),
        VMSTATE_UINT32(agent_version_read_write, VmxPortState),
        VMSTATE_UINT32(fs_version_read_write, VmxPortState),
        
        VMSTATE_UINT32(helper_ret_stat, VmxPortState),
        VMSTATE_UINT8(helper_state, VmxPortState),
        VMSTATE_UINT64(helper_gva, VmxPortState),
        
        
        VMSTATE_UINT8(fs_helper_state, VmxPortState),
        VMSTATE_UINT64(fs_helper_gva, VmxPortState),
        VMSTATE_UINT32(fs_helper_ret_stat, VmxPortState),
        VMSTATE_END_OF_LIST()
    }
};

static const MemAreaOps vmx_port_ops = {
    .read = vmx_port_ioport_read,
    .write = vmx_port_ioport_write,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    //.impl.unaligned = true,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void __port_reset(VmxPortState *vmx_port)
{
    vmx_port->fs_helper_ret_stat = 0;
    vmx_port->helper_ret_stat = 0;
    vmx_port->fs_version_read_write = 0;
    vmx_port->agent_version_read_write = 0;
    vmx_port->mouse_version_read_write = 0;
    vmx_port->mouse_stat.button = 0;
    vmx_port->mouse_activated = 0;
    vmx_port->helper_state = 0;
    vmx_port->helper_gva = 0;
    vmx_port->change_res = 0;
    vmx_port->ack = 1;
}

static void vmx_port_realizefn(DeviceState *dev, Error **errp)
{
    ISADevice *isadev = ISA_DEVICE(dev);
    VmxPortState *s = VMPORT(dev);

    memory_area_init_io(&s->io, VeertuTypeHold(s), &vmx_port_ops, s, "vmx_port", 6);
    isa_register_ioport(isadev, &s->io, VMX_DEBUG_PORT);
    
    vmx_port = s;
    tmp_state = vmx_port;
    __port_reset(vmx_port);

    /* Register some generic port commands */
}

static void vmx_port_reset(DeviceState *dev)
{
    VmxPortState *s = VMPORT(dev);
    
    if (entry) {
        vmx_remove_mouse_event_handler(entry);
        entry = NULL;
    }
    
    __port_reset(s);
}

static void vmx_port_class_initfn(VeertuTypeClassHold *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = vmx_port_realizefn;
    /* Reason: realize sets global port_state */
    dc->cannot_instantiate_with_device_add_yet = true;
    dc->vmsd = &vmstate_vmx;
    dc->reset = vmx_port_reset;
}

static const VeertuTypeInfo vmx_port_info = {
    .name          = TYPE_VMXPORT,
    .parent        = TYPE_ISA_DEVICE,
    .instance_size = sizeof(VmxPortState),
    .class_size =    sizeof(VmxPortState),
    .class_init    = vmx_port_class_initfn,
};

void vmx_port_register_types(void)
{
    register_type_internal(&vmx_port_info);
}
