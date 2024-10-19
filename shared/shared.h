#ifndef __SHARED_H__
#define __SHARED_H__

#include <stdint.h>

#include "../kern/boot.h"
#include "../user/allocator.h"

pcb_t           *pcb_active         __attribute__((section("shared_variables")));
heap_region_t   *heap_free_list     __attribute__((section("shared_variables")));

#endif /* __SHARED_H__ */
