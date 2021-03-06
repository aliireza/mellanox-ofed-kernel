#ifndef _COMPAT_LINUX_RCUPDATE_H
#define _COMPAT_LINUX_RCUPDATE_H

/* Include the autogenerated header file */
#include "../../compat/config.h"

#include_next <linux/rcupdate.h>

#ifndef rcu_replace_pointer
#define rcu_replace_pointer(rcu_ptr, ptr, c)				\
({									\
	typeof(ptr) __tmp = rcu_dereference_protected((rcu_ptr), (c));	\
	rcu_assign_pointer((rcu_ptr), (ptr));				\
	__tmp;								\
})
#endif

#endif /* _COMPAT_LINUX_RCUPDATE_H */
