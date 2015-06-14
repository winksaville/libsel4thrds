/*
 * Copyright 2015, Wink Saville
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 */

#include <stdint.h>

#include <allocman/allocman.h>
#include <allocman/bootstrap.h>

#include <simple/simple.h>
#ifdef CONFIG_KERNEL_STABLE
#include <simple-stable/simple-stable.h>
#else
#include <simple-default/simple-default.h>
#endif
#include "runtime_env.h"

#define RUNTIME_ALLOCATOR_MEM_POOL_SIZE ((1 << seL4_PageBits) * 10)
UNUSED static char runtime_allocator_mem_pool[RUNTIME_ALLOCATOR_MEM_POOL_SIZE];

/* initialise our runtime environment */
void runtime_env_init(runtime_env_t *env)
{
    UNUSED allocman_t *allocman;
    UNUSED reservation_t virtual_reservation;
    UNUSED int error;

    printf("runtime_env_init:+\n");

    UNUSED seL4_BootInfo *info = seL4_GetBootInfo();

    printf("runtime_env_init: 1\n");
#if 0
#ifdef SEL4_DEBUG_KERNEL
    seL4_DebugNameThread(seL4_CapInitThreadTCB, "CapInitThread");
#endif

    /* initialise libsel4simple, which abstracts away which kernel version
     * we are running on */
#ifdef CONFIG_KERNEL_STABLE
    simple_stable_init_bootinfo(&env->simple, info);
#else
    simple_default_init_bootinfo(&env->simple, info);
#endif

    /* create an allocator */
    printf("runtime_env_init:  1\n");
    allocman = bootstrap_use_current_simple(&env->simple, sizeof(runtime_allocator_mem_pool), runtime_allocator_mem_pool);
    assert(allocman);

//#if 0
    /* create a vka (interface for interacting with the underlying allocator) */
    allocman_make_vka(&env->vka, allocman);

    /* create a vspace (virtual memory management interface). We pass
     * boot info not because it will use capabilities from it, but so
     * it knows the address and will add it as a reserved region */
    error = sel4utils_bootstrap_vspace_with_bootinfo_leaky(&env->vspace,
	    &data, simple_get_pd(&env->simple), &env->vka, seL4_GetBootInfo());

    /* fill the allocator with virtual memory */
    void *vaddr;
    virtual_reservation = vspace_reserve_range(&env->vspace,
                                               ALLOCATOR_VIRTUAL_POOL_SIZE, seL4_AllRights, 1, &vaddr);
    assert(virtual_reservation.res);
    bootstrap_configure_virtual_pool(allocman, vaddr,
                                     ALLOCATOR_VIRTUAL_POOL_SIZE, simple_get_pd(&env->simple));
#endif

    printf("runtime_env_init:-\n");
}

