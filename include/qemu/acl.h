/*
 * QEMU access control list management
 *
 * Copyright (C) 2009 Red Hat, Inc
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __QEMU_ACL_H__
#define __QEMU_ACL_H__

#include "qemu/queue.h"

typedef struct vmx_acl_entry vmx_acl_entry;
typedef struct vmx_acl vmx_acl;

struct vmx_acl_entry {
    char *match;
    int deny;

    QTAILQ_ENTRY(vmx_acl_entry) next;
};

struct vmx_acl {
    char *aclname;
    unsigned int nentries;
    QTAILQ_HEAD(,vmx_acl_entry) entries;
    int defaultDeny;
};

vmx_acl *vmx_acl_init(const char *aclname);

vmx_acl *vmx_acl_find(const char *aclname);

int vmx_acl_party_is_allowed(vmx_acl *acl,
			      const char *party);

void vmx_acl_reset(vmx_acl *acl);

int vmx_acl_append(vmx_acl *acl,
		    int deny,
		    const char *match);
int vmx_acl_insert(vmx_acl *acl,
		    int deny,
		    const char *match,
		    int index);
int vmx_acl_remove(vmx_acl *acl,
		    const char *match);

#endif /* __QEMU_ACL_H__ */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
