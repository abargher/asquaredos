#include "scheduler.h"
#include "resources.h"
#include "utils/list.h"

pcb_t *sched_get_next(void) {
    pcb_t *next_pcb;
    DLL_POP(ready_queue, next_pcb, next, prev);
    DLL_PUSH(ready_queue, next_pcb, next, prev);
    return next_pcb->saved_sp;
}