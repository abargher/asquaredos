/*
 * context_switch.s:
 *
 * Implements context switching between processes.
 *
 * Copied almost entirely from a presentation which details the context switch
 * logic for Chromium EC on Cortex M platforms, which can be found here:
 *
 *      https://archive.fosdem.org/2018/schedule/event/multitasking_on_cortexm/attachments/slides/2602/export/events/attachments/multitasking_on_cortexm/slides/2602/Slides.pdf
 *
 * Source code for the relevant functions can be found in the switch.S file
 * in the ChromiumOS source tree here:
 *      https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/core/cortex-m0/switch.S
 * ChromiumOS source code is used with modifications under a BSD-style license:
 */

/* 
 * Copyright 2010 The ChromiumOS Authors
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *    * Neither the name of Google LLC nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

.macro SAVE_CALLER_REGS
    push    {r0-r3}         @ Push r0-r3 onto the stack.
    mov     r0, r12         @ Move r12 (hi) into r0 (lo).
    push    {r0, lr}        @ Push r12 (as r0) and lr onto the stack.
.endm

.macro RESTORE_CALLER_REGS
    pop     {r0}            @ Pop r12 (as r0) off the stack.
    mov     r12, r0         @ Move r0 back into r12.
    pop     {r0}            @ Pop lr (as r0) off the stack.
    mov     lr, r0          @ Move r0 back into lr.
    pop     {r0-r3}         @ Pop r0-r3 and lr off the stack.
.endm

.thumb_func
.global schedule_handler
.align 4
schedule_handler:
    push    {r3, lr}                    @ Save r0-r3, and lr (the saved PC) to the to-be-descheduled process
    bl      sched_get_next              @ Get the next process to load
    ldr     r3, =pcb_active             @ Get the current process
    ldr     r1, [r3]                    @ Dereference pcb_active
    cmp     r0, r1                      @ Check if next to schedule == pcb_active
    beq     schedule_handler_return     @ If we're scheduling the active process then this is a no-op
    str     r0, [r3]                    @ Store the new next to schedule process at pcb_active
    bl      context_switch

.thumb_func
.align 4
schedule_handler_return:
    pop     {r3, pc}                @ Load r0-r3 and and the lr (into the PC) for the to-be-scheduled process

.thumb_func
.align 4
context_switch:
    mrs     r2, psp         @ Write the to-be-descheduled programs's SP into r2
    mov     r3, sp          @ Write the kernel's stack pointer into r3
    mov     sp, r2          @ Activate the to-be-descheduled program's stack, so that we can push registers onto their stack

    /* Save r4-r7 */
    push    {r4-r7}         @ Push registers r4 - r7 onto their stack

    /* Save r8-r11 */
    mov     r4, r8          @ Move r8 - r11 into r4 - r7, respective, because push can't save high registers
    mov     r5, r9          @ ...
    mov     r6, r10         @ ...
    mov     r7, r11         @ ...
    push    {r4-r7}         @ Push the shuffled registers (what were r8 - r11) onto their stack (note typo in the original?)

    /* Save to-be-descheduled SP and load to-be-scheduled SP */
    mov     r2, sp          @ Save the to-be-descheduled program's now-updated SP into r2
    mov     sp, r3          @ Load the kernel's stack pointer from r3 (where we left it)
    str     r2, [r1]        @ Store the to-be-descheduled program's SP into *r1 from r2, i.e., the "saved SP" field in its PCB
    ldr     r2, [r0]        @ Load the to-be-scheduled program's SP from *r0 into r2, i.e., the "saved SP" field of the to-be-scheduled PCB

    /* Prepare the VM system for the new process. */
    mov     r5, r0                          @ We only need the values of r0, r2, and lr after the below function calls
    mov     r6, r2
    mov     r7, lr
    bl      mpu_disable_all_subregions      @ Re-protect all of memory from the new process
    mov     r0, r5                          @ Restore the r0 value we saved earlier
    ldr     r0, [r0, #4]                    @ Load the to-be-scheduled program's page table pointer from *r0+4 (2nd field) into r0, to be the first argument to vm_page_fault
    mov     r1, r6                          @ Load the r2 value we saved earlier into r6 (program's stack pointer, i.e., the faulting address)
    bl      vm_page_fault                   @ Fault in the stack pointer
    mov     r2, r6                          @ Restore the value of r2
    mov     lr, r7                          @ Restore the value of lr

    /* Restore r8-r11 */
    ldmia   r2!, {r4-r7}    @ Using r2 (the saved SP of the to-be-scheduled program), load the saved values of r8-r11 into r4-r7 (inverse of save procedure)
    mov     r8, r4          @ ...
    mov     r9, r5          @ ...
    mov     r10, r6         @ ...
    mov     r11, r7         @ ...

    /* Restore r4-r7 */
    ldmia   r2!, {r4-r7}    @ Using r2 (still the saved SP of the to-be-scheduled program), load the saved values of r4-r7

    /* Load the to-be-scheduled SP into PSP and return to end of handler (to set saved PC) */
    pop     {r0, r1}        @ Pop padding and exc_return value from stack
    mov     r1, #0          @ Ensure that exc_return value is set to 0xfffffffd
    sub     r1, #3          @ ...
    push    {r0, r1}        @ Restore values to stack, now with proper exc_return value.

    msr     psp, r2         @ Save r2 (to-be-scheduled SP) as PSP
    isb                     @ Ensures following instructions use correct stack
    blx     lr              @ Return to schedule_handler_return
