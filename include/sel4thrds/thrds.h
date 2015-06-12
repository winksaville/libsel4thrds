/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */
#ifndef __THRDS_H
#define __THRDS_H

#include <vka/vka.h>
#include <vspace/vspace.h>
#include <sel4utils/thread.h>
#include <sel4utils/process.h>
#include <sel4utils/mapping.h>

#include <sel4platsupport/timer.h>
#include <platsupport/timer.h>

#define THRDS_OUR_PRIO (env->priority)
#define THRDS_WORD_STRING_SIZE 11

/* args provided by the user */
#define THRDS_MAX_ARGS 4
/* metadata helpers adds */
#define THRDS_META     4
/* total args (user + meta) */
#define THRDS_TOTAL_ARGS (THRDS_MAX_ARGS + THRDS_META)

/* max test name size */
#define THRDS_NAME_MAX 20

/* Increase if the sel4test-tests binary
 * has new loadable sections added */
#define THRDS_MAX_REGIONS 4


typedef struct thrd_env {
    /* An initialised vka that may be used by the test. */
    vka_t vka;
    /* virtual memory management interface */
    vspace_t vspace;
    /* initialised timer */
    seL4_timer_t *timer;
    /* abstract interface over application init */
    simple_t simple;
    /* aep for timer */
    vka_object_t timer_aep;

    /* caps for the current process */
    seL4_CPtr cspace_root;
    seL4_CPtr page_directory;
    seL4_CPtr endpoint;
    seL4_CPtr tcb;
#ifndef CONFIG_KERNEL_STABLE
    seL4_CPtr asid_pool;
#endif /* CONFIG_KERNEL_STABLE */
#ifdef CONFIG_IOMMU
    seL4_CPtr io_space;
#endif /* CONFIG_IOMMU */
    seL4_CPtr domain;

    int priority;
    int cspace_size_bits;
    int num_regions;
    sel4utils_elf_region_t regions[THRDS_MAX_REGIONS];
} thrd_env_t;


typedef struct thrd {
    bool is_process;
    sel4utils_process_t process;
    sel4utils_thread_t thread;
    vka_object_t local_endpoint;
    seL4_CPtr fault_endpoint;

    int num_regions;
    sel4utils_elf_region_t regions[THRDS_MAX_REGIONS];

    char name[THRDS_NAME_MAX];
    void *arg0;
    void *arg1;
    char *args[THRDS_TOTAL_ARGS];
    char args_strings[THRDS_TOTAL_ARGS][THRDS_WORD_STRING_SIZE];
} thrd_t;

typedef int (*thrd_fn_t)(seL4_Word, seL4_Word, seL4_Word, seL4_Word);

/* do nothing */
void thrd_doNothing();

/* Configure a thrd in the current vspace and current cspace */
void thrd_configure(thrd_env_t *env, thrd_t *thrd);

/* Configure a process which is a clone of the current vspace loadable elf segments,
 * and a new cspace */
void thrd_process_configure(thrd_env_t *env, thrd_t *thrd);

/* set a helper threads priority */
void thrd_set_priority(thrd_t *thrd, seL4_Word prio);

/* Start a thrd. Note: arguments to processes will be copied into
 * the address space of that process. Do not pass pointers to data only in
 * the local vspace, this will fail. */
void thrd_start(thrd_env_t *env, thrd_t *thread, thrd_fn_t entry_point,
                  seL4_Word arg0, seL4_Word arg1, seL4_Word arg2, seL4_Word arg3);

/* wait for a thrd to finish */
int thrd_wait(thrd_env_t *env);

/* free all resources associated with a helper and tear it down */
void thrd_cleanup(thrd_env_t *env, thrd_t *thrd);

/* timer */
void thrd_wait_for_timer_interrupt(thrd_env_t *env);

#endif /* __HELPERS_H */
