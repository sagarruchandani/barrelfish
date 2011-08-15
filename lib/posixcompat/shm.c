/*
 * Copyright (c) 2011, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdlib.h>
#include <sys/shm.h>
#include <assert.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <barrelfish/barrelfish.h>
#include <barrelfish/nameservice_client.h>
#include "fdtab.h"
#include "posixcompat.h"

struct _shm {
    struct shmid_ds ds;
    bool used;
    key_t key;
    void *mem;
    struct capref frame;
};

#define MAX_SHMS        32

struct _shm shms[MAX_SHMS];

void *shmat(int shmid, const void *shmaddr, int shmflg)
{
    assert(shmid >= 0 && shmid < MAX_SHMS);
    struct _shm *s = &shms[shmid];
    errval_t err;

    if(!s->used) {
        errno = EINVAL;
        return NULL;
    }

    s->ds.shm_nattch++;

    struct frame_identity id;
    vregion_flags_t attr;

    err = invoke_frame_identify(s->frame, &id);
    if(err_is_fail(err)) {
        USER_PANIC_ERR(err, "invoke_frame_identify");
    }

    if(shmflg & SHM_RDONLY) {
        attr = VREGION_FLAGS_READ;
    } else {
        attr = VREGION_FLAGS_READ_WRITE;
    }

    if(shmaddr != NULL) {
        err = vspace_map_one_frame_fixed_attr((lvaddr_t)shmaddr, 1 << id.bits,
                                              s->frame, attr, NULL, NULL);
        s->mem = (void *)shmaddr;
    } else {
        err = vspace_map_one_frame_attr(&s->mem, 1 << id.bits, s->frame,
                                        attr, NULL, NULL);
    }

    if(err_is_fail(err)) {
        DEBUG_ERR(err, "vspace_map_one_frame_(fixed_)attr");
        // XXX: Probably cause it didn't fit the virtual address space
        errno = ENOMEM;
        return NULL;
    }

    POSIXCOMPAT_DEBUG("shmat(%d, %p, %d) = %p\n",
                      shmid, shmaddr, shmflg, s->mem);
    return s->mem;
}

int shmget(key_t key, size_t size, int shmflg)
{
    int i;

    // XXX: Private key not supported yet
    assert(key != IPC_PRIVATE);

    POSIXCOMPAT_DEBUG("key is not ipc_private\n");
    for(i = 0; i < MAX_SHMS; i++) {
        if(shms[i].used && shms[i].key == key) {
            break;
        }
    }

    if(i == MAX_SHMS) {
        // Allocate an SHM descriptor
        for(i = 0; i < MAX_SHMS; i++) {
            if(!shms[i].used) {
                break;
            }
        }
    }

    if(i == MAX_SHMS) {
        // Out of descriptors
        errno = ENOSPC;
        return -1;
    }
    
    struct _shm *s = &shms[i];
    char skey[128];
    snprintf(skey, 128, "%lu", key);

    POSIXCOMPAT_DEBUG("nameservice get capability %s\n", skey);
    // XXX: Not multi-processing safe!
    errval_t err = nameservice_get_capability(skey, &s->frame);
    POSIXCOMPAT_DEBUG("nameservice returned!\n");

    if(err_is_fail(err) && err_no(err) != CHIPS_ERR_UNKNOWN_NAME) {
        USER_PANIC_ERR(err, "nameservice_get_capability");
    }

    if(err == CHIPS_ERR_UNKNOWN_NAME) {
        if(!(shmflg & IPC_CREAT)) {
            errno = ENOENT;
            return -1;
        }

        // Allocate frame (don't map it yet)
        err = frame_alloc(&s->frame, size, NULL);
        if(err_is_fail(err)) {
            DEBUG_ERR(err, "frame_alloc");
            if(err_no(err) == LIB_ERR_RAM_ALLOC_MS_CONSTRAINTS) {
                errno = ENOMEM;
            }
            return -1;
        }

        // XXX: This can fail if someone else won the race
        err = nameservice_put_capability(skey, s->frame);
        if(err_is_fail(err)) {
            USER_PANIC_ERR(err, "nameservice_put_capability");
        }
    }

    // Assign to local cache
    s->used = true;
    s->key = key; 

    POSIXCOMPAT_DEBUG("shmget(%ld, %zu, %d) = %d\n",
                      key, size, shmflg, i);

    return i;
}

int shmctl(int shmid, int cmd, struct shmid_ds *buf)
{
    assert(shmid >= 0 && shmid < MAX_SHMS);
    struct _shm *s = &shms[shmid];

    if(!s->used) {
        return -1;
    }

    POSIXCOMPAT_DEBUG("shmctl(%d, %d, %p)\n", shmid, cmd, buf);

    switch(cmd) {
    case IPC_STAT:
        assert(!"NYI");
        break;

    case IPC_SET:
        assert(!"NYI");
        break;

    case IPC_RMID:
        assert(!"NYI");
        break;

    default:
        return -1;
    }

    return 0;
}

int shmdt(const void *shmaddr)
{
    struct _shm *s = NULL;
    errval_t err;

    for(int i = 0; i < MAX_SHMS; i++) {
        if(shms[i].used && shms[i].mem == shmaddr) {
            s = &shms[i];
            break;
        }
    }

    if(s == NULL) {
        errno = EINVAL;
        return -1;
    }

    POSIXCOMPAT_DEBUG("shmdt(%p)\n", shmaddr);

    err = vspace_unmap(shmaddr);
    if(err_is_fail(err)) {
        DEBUG_ERR(err, "vspace_unmap");
        return -1;
    }

    s->ds.shm_nattch--;
    return 0;
}