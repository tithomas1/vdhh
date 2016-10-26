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

#import <CoreServices/CoreServices.h>
#include <IOKit/IOKitLib.h>
#include "utils.h"
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <time.h>
#include <sys/param.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IODVDMedia.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOCDTypes.h>
#include <IOKit/storage/IOMediaBSDClient.h>
#include <paths.h>
#include <sys/mount.h>

char conf_name[16][256];
static int dev_index = 0;
static int cur_index = 0;

void set_current_conf_name(char *new_conf_name)
{
    printf("new conf name is %s\n", new_conf_name);
    strcpy(conf_name[dev_index++], new_conf_name);
}

char *get_current_conf_name()
{
    return conf_name[cur_index++];
}

ssize_t readLine(int fd, void *buffer, size_t n)
{
    ssize_t numRead;                    /* # of bytes fetched by last read() */
    size_t totRead;                     /* Total bytes read so far */
    char *buf;
    char ch;
    
    if (n <= 0 || buffer == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    buf = buffer;                       /* No pointer arithmetic on "void *" */
    
    totRead = 0;
    for (;;) {
        numRead = read(fd, &ch, 1);
        
        if (numRead == -1) {
            if (errno == EINTR)         /* Interrupted --> restart read() */
                continue;
            else
                return -1;              /* Some other error */
            
        } else if (numRead == 0) {      /* EOF */
            if (totRead == 0)           /* No bytes read; return 0 */
                return 0;
            else                        /* Some bytes read; add '\0' */
                break;
            
        } else {                        /* 'numRead' must be 1 if we get here */
            if (totRead < n - 1) {      /* Discard > (n - 1) bytes */
                totRead++;
                *buf++ = ch;
            }
            
            if (ch == '\n')
                break;
        }
    }
    
    *buf = '\0';
    return totRead;
}

void generate_macaddr(uint8_t *macaddr)
{
    macaddr[0] = 'v';
    macaddr[1] = 'm';
    macaddr[2] = 'x';

    srandom((unsigned int)time(NULL));
    macaddr[3] = random();
    macaddr[4] = random();
    macaddr[5] = random();
}

char *macaddr_to_string(uint8_t *macaddr, char *buf)
{
    sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x", macaddr[0], macaddr[1], macaddr[2],
                                                macaddr[3], macaddr[4], macaddr[5]);
    return buf;
}

/*
void get_platform_uuid(char *buf, int bufSize)
{
    io_registry_entry_t ioRegistryRoot = IORegistryEntryFromPath(kIOMasterPortDefault, "IOService:/");
    CFStringRef uuidCf = (CFStringRef) IORegistryEntryCreateCFProperty(ioRegistryRoot, CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault, 0);
    IOObjectRelease(ioRegistryRoot);
    CFStringGetCString(uuidCf, buf, bufSize, kCFStringEncodingMacRoman);
    CFRelease(uuidCf);
}
*/

static bool FindEjectableCDMedia(io_iterator_t *mediaIterator, const char *media_type)
{
    mach_port_t         masterPort;
    kern_return_t       kernResult;
    CFMutableDictionaryRef   classesToMatch;
    
    kernResult = IOMasterPort( MACH_PORT_NULL, &masterPort );
    if ( kernResult != KERN_SUCCESS )
        return false;

    // CD media are instances of class kIOCDMediaClass.
    classesToMatch = IOServiceMatching(media_type);
    if (!classesToMatch)
        return false;
    else
    {
        // Each IOMedia object has a property with key kIOMediaEjectableKey
        // which is true if the media is indeed ejectable. So add this
        // property to the CFDictionary for matching.
        CFDictionarySetValue(classesToMatch, CFSTR( kIOMediaEjectableKey ), kCFBooleanTrue );
    }
    kernResult = IOServiceGetMatchingServices( masterPort, classesToMatch, mediaIterator);
    return (kernResult == KERN_SUCCESS);
}

static bool GetDeviceProperty(io_object_t nextMedia, CFStringRef prop, char *deviceFilePath, CFIndex maxPathSize)
{
    kern_return_t kernResult = KERN_FAILURE;
    
    *deviceFilePath = '\0';
    if (nextMedia)
    {
        CFTypeRef   deviceFilePathAsCFString;
        deviceFilePathAsCFString = IORegistryEntryCreateCFProperty(
                                                                   nextMedia, prop,
                                                                   kCFAllocatorDefault, 0 );
        *deviceFilePath = '\0';
        if ( deviceFilePathAsCFString )
        {
            size_t devPathLength;
            strcpy(deviceFilePath, _PATH_DEV );
            // Add "r" before the BSD node name from the I/O Registry
            // to specify the raw disk node. The raw disk node receives
            // I/O requests directly and does not go through the
            // buffer cache.
            strcat( deviceFilePath, "r");
            devPathLength = strlen( deviceFilePath );
            if (CFStringGetCString(deviceFilePathAsCFString,
                                    deviceFilePath + devPathLength,
                                    maxPathSize - devPathLength,
                                    kCFStringEncodingASCII)) {
                kernResult = KERN_SUCCESS;
            }
            CFRelease( deviceFilePathAsCFString );
        }
    }
    return (kernResult == KERN_SUCCESS);
}


static const char *GetFileSystemStatusFromMount(const char *mount)
{
    struct statfs * mountList;
    int mountListCount;
    int mountListIndex;
    char cmount[MAXPATHLEN] = {0};
    static char cdrom[] = "CD-ROM";

    // skip 'r' in /dev/rdiskXXX
    memset(cmount, 0, sizeof(cmount));
    const char *p = strchr(mount+1, '/');
    if (!p)
        return NULL;
    strncpy(cmount, mount, p - mount + 1);
    strcat(cmount, p + 2);

    mountListCount = getmntinfo(&mountList, MNT_NOWAIT);
    
    for (mountListIndex = 0; mountListIndex < mountListCount; mountListIndex++) {
        if (strcmp(mountList[mountListIndex].f_mntfromname, cmount) == 0) {
            return mountList[mountListIndex].f_mntonname;
        }
    }
    
    return cdrom;
}


CFMutableArrayRef get_cdroms()
{
    io_iterator_t mediaIterator;
    io_object_t nextMedia;
    char deviceFilePath[MAXPATHLEN];
    char* media_types[] = {kIOCDMediaClass, kIODVDMediaClass};

    CFMutableArrayRef anArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    
    for (int i = 0; i < 2; i++) {
        if (FindEjectableCDMedia(&mediaIterator, media_types[i])) {
            while ((nextMedia = IOIteratorNext(mediaIterator))) {

                if (GetDeviceProperty(nextMedia, CFSTR(kIOBSDNameKey), deviceFilePath, sizeof(deviceFilePath))) {
                    const char *mnt_path = GetFileSystemStatusFromMount(deviceFilePath);

                    CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                    CFDictionaryAddValue(d, CFSTR("dev"),
                        CFStringCreateWithCString(NULL, deviceFilePath, kCFStringEncodingASCII));
                    CFDictionaryAddValue(d, CFSTR("mnt"),
                        CFStringCreateWithCString(NULL, mnt_path, kCFStringEncodingASCII));
                    CFArrayAppendValue(anArray, d);
                }
                IOObjectRelease(nextMedia);
            }
        }
        IOObjectRelease(mediaIterator);
    }
    return  anArray;
}

bool osx_is_sierra()
{
    SInt32 major, minor, bugfix;
    Gestalt(gestaltSystemVersionMajor, &major);
    Gestalt(gestaltSystemVersionMinor, &minor);
    Gestalt(gestaltSystemVersionBugFix, &bugfix);
    if (major >= 10 && minor >= 12)
        return true;
    return false;
}
