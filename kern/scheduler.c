#include "scheduler.h"
#include "resources.h"
#include "utils/list.h"
#include <stdio.h>

/*
Choose the next process to be scheduled in a round-robin fashion. Places the
next process's PCB back on the end of the ready queue.
*/
pcb_t *sched_get_next(void) {
    pcb_t *next_pcb;
    DLL_POP(ready_queue, next_pcb, next, prev);
    DLL_PUSH(ready_queue, next_pcb, next, prev);
    return next_pcb;
}