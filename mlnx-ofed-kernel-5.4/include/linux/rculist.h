#ifndef _COMPAT_LINUX_RCULIST_H
#define _COMPAT_LINUX_RCULIST_H

/* Include the autogenerated header file */
#include "../../compat/config.h"

#include_next <linux/rculist.h>

#define compat_hlist_for_each_entry_rcu(pos, head, member) \
	hlist_for_each_entry_rcu(pos, head, member)

#ifndef list_next_or_null_rcu
/**
 * list_next_or_null_rcu - get the first element from a list
 * @head:       the head for the list.
 * @ptr:        the list head to take the next element from.
 * @type:       the type of the struct this is embedded in.
 * @member:     the name of the list_head within the struct.
 *
 * Note that if the ptr is at the end of the list, NULL is returned.
 *
 * This primitive may safely run concurrently with the _rcu list-mutation
 * primitives such as list_add_rcu() as long as it's guarded by rcu_read_lock().
 */
#define list_next_or_null_rcu(head, ptr, type, member) \
({ \
        struct list_head *__head = (head); \
        struct list_head *__ptr = (ptr); \
        struct list_head *__next = READ_ONCE(__ptr->next); \
        likely(__next != __head) ? list_entry_rcu(__next, type, \
                                                  member) : NULL; \
})
#endif /* ! list_next_or_null_rcu */

#endif /* _COMPAT_LINUX_RCULIST_H */
