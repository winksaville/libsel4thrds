/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */


#include <sel4/sel4.h>
#include <sel4utils/arch/util.h>

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <utils/util.h>
#include <vka/capops.h>

#include <sel4thrds/thrds.h>

/**
 * Create the arguments
 */
static void create_args(char strings[][THRDS_WORD_STRING_SIZE], char *argv[], int argc, ...) {
    va_list args;
    va_start(args, argc);

    for (int i = 0; i < argc; i++) {
        seL4_Word arg = va_arg(args, seL4_Word);
        argv[i] = strings[i];
        snprintf(argv[i], THRDS_WORD_STRING_SIZE, "%d", arg);

    }

    va_end(args);
}

/**
 * Signal that a thrd has finished
 */
static void signal_thrd_finished(seL4_CPtr local_endpoint, int val)
{
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_SetMR(0, val);
    seL4_Call(local_endpoint, info);
    assert(0);
    while (1);
}

/**
 * Invoke the thrd entry_point
 */
static void invoke_thrd(int argc, char **argv) {

    thrd_fn_t entry_point = (void *) atoi(argv[2]);
    seL4_CPtr local_endpoint = (seL4_CPtr) atoi(argv[3]);

    seL4_Word args[THRDS_MAX_ARGS] = {0};
    for (int i = 4; i < argc && i - 4 < THRDS_MAX_ARGS; i++) {
        assert(argv[i] != NULL);
        args[i - 4] = atoi(argv[i]);
    }

    /* run the thread */
    int result = entry_point(args[0], args[1], args[2], args[3]);
    signal_thrd_finished(local_endpoint, result);
    /* does not return */
}

#include <stdio.h>

void thrd_doNothing() {
  printf("thrd_doNothing:+-\n");
}

/**
 * Configure a thrd
 */
void thrd_configure(thrd_env_t *env, thrd_t *thread) {
    UNUSED int error;

    error = vka_alloc_endpoint(&env->vka, &thread->local_endpoint);
    assert(error == 0);

    thread->is_process = false;
    thread->fault_endpoint = env->endpoint;
    seL4_CapData_t data = seL4_CapData_Guard_new(0, seL4_WordBits - env->cspace_size_bits);
    error = sel4utils_configure_thread(&env->vka, &env->vspace, &env->vspace, env->endpoint,
                                       THRDS_OUR_PRIO - 1, env->cspace_root, data, &thread->thread);
    assert(error == 0);
}

/**
 * Configure a new process
 */
void thrd_process_configure(thrd_env_t *env, thrd_t *thread) {
    UNUSED int error;

    error = vka_alloc_endpoint(&env->vka, &thread->local_endpoint);
    assert(error == 0);

    thread->is_process = true;

    sel4utils_process_config_t config = {
        .is_elf = false,
        .create_cspace = true,
        .one_level_cspace_size_bits = env->cspace_size_bits,
        .create_vspace = true,
        .reservations = env->regions,
        .num_reservations = env->num_regions,
        .create_fault_endpoint = false,
        .fault_endpoint = { .cptr = env->endpoint },
        .priority = THRDS_OUR_PRIO - 1,
#ifndef CONFIG_KERNEL_STABLE
        .asid_pool = env->asid_pool,
#endif
    };

    error = sel4utils_configure_process_custom(&thread->process, &env->vka, &env->vspace,
                                               config);
    assert(error == 0);

    /* copy the elf reservations we need into the current process */
    memcpy(thread->regions, env->regions, sizeof(sel4utils_elf_region_t) * env->num_regions);
    thread->num_regions = env->num_regions;

    /* clone data/code into vspace */
    for (int i = 0; i < env->num_regions; i++) {
        error = sel4utils_bootstrap_clone_into_vspace(&env->vspace, &thread->process.vspace, thread->regions[i].reservation);
        assert(error == 0);
    }

    thread->thread = thread->process.thread;
    assert(error == 0);
}

/** Address of the process entry_point */
extern uintptr_t _start[];

/** Address of the process sysinfo */
extern uintptr_t sel4_vsyscall[];

/**
 * Start a thrd
 */
void thrd_start(thrd_env_t *env, thrd_t *thread, thrd_fn_t entry_point,
             seL4_Word arg0, seL4_Word arg1, seL4_Word arg2, seL4_Word arg3) {

    UNUSED int error;

    seL4_CPtr local_endpoint;

    if (thread->is_process) {
        /* copy the local endpoint */
        cspacepath_t path;
        vka_cspace_make_path(&env->vka, thread->local_endpoint.cptr, &path);
        local_endpoint = sel4utils_copy_cap_to_process(&thread->process, path);
    } else {
        local_endpoint = thread->local_endpoint.cptr;
    }

    /* If we are starting a process then the first two args are to get us
     * through the standard 'main' function and end up in invoke_thrd
     * if we are starting a regular thread then these will be ignored */
    create_args(thread->args_strings, thread->args, THRDS_TOTAL_ARGS,
        0, invoke_thrd, (seL4_Word) entry_point, local_endpoint,
        arg0, arg1, arg2, arg3);

    if (thread->is_process) {
        thread->process.entry_point = (void*)_start;
        thread->process.sysinfo = (uintptr_t)sel4_vsyscall;
        error = sel4utils_spawn_process_v(&thread->process, &env->vka, &env->vspace,
                                        THRDS_TOTAL_ARGS, thread->args, 1);
        assert(error == 0);
    } else {
        error = sel4utils_start_thread(&thread->thread, invoke_thrd,
                                       (void *) THRDS_TOTAL_ARGS, (void *) thread->args, 1);
        assert(error == 0);
    }
}

/**
 * Clean up after a thrd has finished
 */
void thrd_cleanup(thrd_env_t *env, thrd_t *thread) {

    vka_free_object(&env->vka, &thread->local_endpoint);

    if (thread->is_process) {
        /* free the regions (no need to unmap, as the
        * entry address space / cspace is being destroyed */
        for (int i = 0; i < thread->num_regions; i++) {
            vspace_free_reservation(&thread->process.vspace, thread->regions[i].reservation);
        }

        thread->process.fault_endpoint.cptr = 0;
        sel4utils_destroy_process(&thread->process, &env->vka);
    } else {
        sel4utils_clean_up_thread(&env->vka, &env->vspace, &thread->thread);
    }
}


/**
 * Wait for another thrd to finish
 */
int wait_for_helper(thrd_t *thread) {
    seL4_Word badge;

    seL4_Wait(thread->local_endpoint.cptr, &badge);
    return seL4_GetMR(0);
}

/**
 * Set a Thrd's priority
 */
void thrd_set_priority(thrd_t *thread, seL4_Word prio) {
    UNUSED int error;
    error = seL4_TCB_SetPriority(thread->thread.tcb.cptr, prio);
    assert(error == seL4_NoError);
}

/**
 * Wait for the timer interrupt
 */
void thrd_wait_for_timer_interrupt(thrd_env_t *env) {
    seL4_Word sender_badge;
    seL4_Wait(env->timer_aep.cptr, &sender_badge);
    sel4_timer_handle_single_irq(env->timer);
}
