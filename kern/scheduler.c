#include "scheduler.h"
#include "resources.h"
#include "utils/list.h"

pcb_t *sched_get_next(void) {
    pcb_t *next_pcb;
    printf("ready_queue = %p\n", ready_queue);
    DLL_POP(ready_queue, next_pcb, next, prev);
    printf("ready_queue = %p\n", ready_queue);
    DLL_PUSH(ready_queue, next_pcb, next, prev);
    printf("ready_queue = %p\n", ready_queue);
    printf("next_pcb = %p, next_pcb->saved_sp = %p\n", next_pcb, next_pcb->saved_sp);

    return next_pcb;
}