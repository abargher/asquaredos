#include "scheduler.h"
#include "resources.h"
#include "utils/list.h"
#include <stdio.h>

pcb_t *sched_get_next(void) {
    pcb_t *next_pcb;
    char buf[128];
    sprintf(buf, "ready_queue = %p\n", ready_queue);
    write(stdout, buf);
    DLL_POP(ready_queue, next_pcb, next, prev);
    sprintf(buf, "ready_queue = %p\n", ready_queue);
    write(stdout, buf);
    DLL_PUSH(ready_queue, next_pcb, next, prev);
    sprintf(buf, "ready_queue = %p\n", ready_queue);
    write(stdout, buf);
    sprintf(buf, "next_pcb = %p, next_pcb->saved_sp = %p\n", next_pcb, next_pcb->saved_sp);
    write(stdout, buf);
    fflush(stdout);

    return next_pcb;
}