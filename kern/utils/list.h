/*
 * list.h:
 *
 * Defines link list and queue utility macros.
 */

#ifndef __LIST_H__
#define __LIST_H__

/*
 * Pop an element from the head of a single link list (FIFO). Sets out to NULL
 * if the list is empty.
 *
 * - head:  points to first element in the list.
 * - tail:  points to the last element in the list.
 * - out:   will be pointed to the popped element.
 * - next:  name of "next" field in the list struct.
 */
#define SLL_POP(head, tail, out, next)                                         \
    do {                                                                       \
        if ((head) == NULL) {                                                  \
            (out) = NULL;                                                      \
            break;                                                             \
        }                                                                      \
        (out) = (head);                                                        \
        (head) = (head)->next;                                                 \
        if ((head) == NULL) {   /* We popped from a singleton; tail changed. */\
            (tail) = NULL;                                                     \
        }                                                                      \
    } while (0)

/*
 * Push an element onto the tail of a single link list (FIFO).
 *
 * - head:  points to first element in the list.
 * - tail:  points to the last element in the list.
 * - elem:  pointer to element to be pushed onto list.
 * - next:  name of "next" field in the list struct.
 */
#define SLL_PUSH(head, tail, elem, next)                                       \
    do {                                                                       \
        (elem)->next = NULL;                                                   \
        if ((tail) == NULL) {   /* Empty list. */                              \
            (head) = (elem);                                                   \
        } else {                /* Non-empty list. */                          \
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
 * - head:  points to first element in the list.
 * - out:   will be pointed to the popped element.
 * - next:  name of "next" field in the list struct.
 * - prev:  name of "prev" field in the list struct.
 */
#define DLL_POP(head, out, next, prev)                                         \
    do {                                                                       \
        (out) = (head);                                                        \
        if ((head) == NULL) {           /* Empty list. */                      \
            break;                                                             \
        }                                                                      \
        if ((head) == (head)->prev) {   /* Singleton list. */                  \
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
 * - head:  points to first element in the list.
 * - elem:  points to the element to be pushed.
 * - next:  name of "next" field in the list struct.
 * - prev:  name of "prev" field in the list struct.
 */
#define DLL_PUSH(head, elem, next, prev)                                       \
    do {                                                                       \
        if ((head) == NULL) {   /* Empty list. */                              \
            (head) = (elem);                                                   \
            (elem)->prev = (elem);                                             \
        } else {                /* Not-empty list. */                          \
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
 * - head:  points to first element in the list.
 * - after: points to the element after where "elem" should be inserted.
 * - elem:  points to the element to be inserted.
 * - next:  name of "next" field in the list struct.
 * - prev:  name of "prev" field in the list struct.
 */
#define DLL_INSERT(head, after, elem, next, prev)                              \
    do {                                                                       \
        if ((after) == NULL) {  /* Make the element the new head.*/            \
            (elem)->next = (head);                                             \
            if ((head) == NULL) {                                              \
                (elem)->prev = (elem);                                         \
            } else {                                                           \
                (elem)->prev = (head)->prev;                                   \
                (head)->prev = (elem);                                         \
            }                                                                  \
            (head) = elem;                                                     \
            break;                                                             \
        }                                                                      \
        if ((after)->next == NULL) {    /* Make the element the new tail. */   \
            (head)->prev = (elem);                                             \
        } else {                                                               \
            (after)->next->prev = (elem);                                      \
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
 * - head:  points to first element in the list.
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
