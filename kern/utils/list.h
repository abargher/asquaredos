/*
 * list.h:
 *
 * Defines link list and queue utility macros.
 */

#ifndef __LIST_H__
#define __LIST_H__

/*
 * Pop an element from the head of a single link list (FIFO). Sets out to NULL
 * if the queue is empty.
 *
 * - head:  points to first element in the queue.
 * - out:   will be pointed to the popped element.
 * - next:  name of "next" field in the queue struct.
 */
#define SLL_POP(head, out, next)                                               \
    do {                                                                       \
        if ((head) == NULL) {                                                  \
            (out) = NULL;                                                      \
            break;                                                             \
        }                                                                      \
        (out) = (head);                                                        \
        (head) = (head)->next;                                                 \
    } while (0)

/*
 * Push an element onto the tail of a single link list (FIFO).
 *
 * - tail:  points to the last element in the queue.
 * - elem:  pointer to element to be pushed onto queue.
 * - next:  name of "next" field in the queue struct.
 */
#define SLL_PUSH(head, tail, elem, next)                                       \
    do {                                                                       \
        (elem)->next = NULL;                                                   \
        if ((tail) == NULL) {                                                  \
            (head) = (elem);                                                   \
        } else {                                                               \
            (tail)->next = (elem);                                             \
        }                                                                      \
        (tail) = (elem);                                                       \
    } while (0)

/*
 * Pop an element from the head of a double link list (FIFO). Sets out to NULL
 * if the list is empty. Note that forward ("next") pointers are non-circular,
 * and backward ("prev") pointers are circular. Thus, x->prev will never be
 * NULL.
 *
 * - head:  points to first element in the queue.
 * - out:   will be pointed to the popped element.
 * - next:  name of "next" field in the queue struct.
 * - prev:  name of "prev" field in the queue struct.
 */
#define DLL_POP(head, out, next, prev)                                         \
    do {                                                                       \
        (out) = (head);                                                        \
        if ((head) == NULL) {           /* Empty queue. */                     \
            break;                                                             \
        }                                                                      \
        if ((head) == (head)->prev) {   /* Singleton queue. */                 \
            (head) = NULL;                                                     \
            break;                                                             \
        }                                                                      \
        (head) = (out)->next;                                                  \
        (head)->prev = (out)->prev;                                            \
    } while (0)

/*
 * Push an element to the tail of a double link list (FIFO). Sets out to NULL if
 * the list is empty. Note that forward ("next") pointers are non-circular, and
 * backward ("prev") pointers are circular. Thus, x->prev will never be NULL.
 *
 * - head:  points to first element in the queue.
 * - out:   will be pointed to the popped element.
 * - next:  name of "next" field in the list struct.
 * - prev:  name of "prev" field in the list struct.
 */
#define DLL_PUSH(head, elem, next, prev)                                       \
    do {                                                                       \
        if ((head) == NULL) {   /* Empty queue. */                             \
            (head) = (elem);                                                   \
            (elem)->prev = (elem);                                             \
        } else {                /* Not-empty queue. */                         \
            (elem)->prev = (head)->prev;                                       \
            (head)->prev->next = (elem);                                       \
        }                                                                      \
        (head)->prev = elem;                                                   \
        (elem)->next = NULL;                                                   \
    } while (0)

/*
 * Insert an element after another element in a double link list. Note that
 * forward ("next") pointers are non-circular, and backward ("prev") pointers
 * are circular. Thus, x->prev will never be NULL.
 *
 * - head:  points to first element in the queue.
 * - after: points to the element after where "elem" should be inserted.
 * - elem:  points to the element to be inserted.
 * - next:  name of "next" field in the list struct.
 * - prev:  name of "prev" field in the list struct.
 */
#define DLL_INSERT(head, after, elem, next, prev)                              \
    do {                                                                       \
        if ((after) == NULL) {  /* Make the element the new head.*/            \
            (elem)->next = (head);                                             \
            (elem)->prev = (head) ? (head)->prev : NULL;                       \
            (head) = elem;                                                     \
            break;                                                             \
        }                                                                      \
        if ((after) == (head)->prev) {    /* Make the element the new tail. */ \
            (head)->prev = (elem);                                             \
        }                                                                      \
        (elem)->next = (after)->next;                                          \
        (elem)->prev = (after);                                                \
        (after)->next = (elem);                                                \
    } while (0)

/*
 * Remove an arbitary element from a double link list. Note that forward
 * ("next") pointers are non-circular, and backward ("prev") pointers are
 * circular. Thus, x->prev will never be NULL.
 *
 * - head:  points to first element in the queue.
 * - elem:  points to the element to be removed.
 * - next:  name of "next" field in the list struct.
 * - prev:  name of "prev" field in the list struct.
 */
#define DLL_REMOVE(head, elem, next, prev)                                     \
    do {                                                                       \
        if ((elem) == (head)) { /* Removing the head. */                       \
            DLL_POP(head, elem, next, prev);                                   \
            break;                                                             \
        }                                                                      \
        if ((elem) == (head)->prev) {   /* Last elem -> update head backptr. */\
            (head)->prev = (elem)->prev;                                       \
        } else {    /* Not last elem -> update next's backptr. */              \
            (elem)->next->prev = (elem)->prev;                                 \
        }                                                                      \
        (elem)->prev->next = (elem)->next;                                     \
    } while (0)

#endif /* __LIST_H__ */
