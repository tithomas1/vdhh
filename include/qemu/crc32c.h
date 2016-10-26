/* crc32c.h -- compute CRC-32C using the Intel crc32 instruction
 * Copyright (C) 2013 Mark Adler
 * Version 1.1  1 Aug 2013  Mark Adler
 */

/*
 This software is provided 'as-is', without any express or implied
 warranty.  In no event will the author be held liable for any damages
 arising from the use of this software.
 
 Permission is granted to anyone to use this software for any purpose,
 including commercial applications, and to alter it and redistribute it
 freely, subject to the following restrictions:
 
 1. The origin of this software must not be misrepresented; you must not
 claim that you wrote the original software. If you use this software
 in a product, an acknowledgment in the product documentation would be
 appreciated but is not required.
 2. Altered source versions must be plainly marked as such, and must not be
 misrepresented as being the original software.
 3. This notice may not be removed or altered from any source distribution.
 
 Mark Adler
 madler@alumni.caltech.edu
 */

/* Use hardware CRC instruction on Intel SSE 4.2 processors.  This computes a
 CRC-32C, *not* the CRC-32 used by Ethernet and zip, gzip, etc.  A software
 version is provided as a fall-back, as well as for speed comparisons. */

/* Version history:
 1.0  10 Feb 2013  First version
 1.1   1 Aug 2013  Correct comments on why three crc instructions in parallel
 */

#ifndef QEMU_CRC32C_H
#define QEMU_CRC32C_H

#include "qemu-common.h"

uint32_t crc32c(uint32_t crc, const uint8_t *data, unsigned int length);

#endif
