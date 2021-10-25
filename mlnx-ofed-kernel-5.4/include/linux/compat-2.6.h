#ifndef LINUX_26_COMPAT_H
#define LINUX_26_COMPAT_H

#define LINUX_BACKPORT(__sym) backport_ ##__sym

/* Include the autogenerated header file */
#include "../../compat/config.h"

#include <linux/version.h>
#include <linux/kconfig.h>
#include <linux/if.h>
#include <linux/compat_autoconf.h>
#include <linux/compat_fix.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/uidgid.h>

/*
 * The define overwriting module_init is based on the original module_init
 * which looks like this:
 * #define module_init(initfn)					\
 *	static inline initcall_t __inittest(void)		\
 *	{ return initfn; }					\
 *	int init_module(void) __attribute__((alias(#initfn)));
 *
 * To the call to the initfn we added the symbol dependency on compat
 * to make sure that compat.ko gets loaded for any compat modules.
 */
void backport_dependency_symbol(void);

#ifndef __has_attribute
# define __has_attribute(x) __GCC4_has_attribute_##x
# define __GCC4_has_attribute___assume_aligned__      (__GNUC_MINOR__ >= 9)
# define __GCC4_has_attribute___copy__                0
# define __GCC4_has_attribute___designated_init__     0
# define __GCC4_has_attribute___externally_visible__  1
# define __GCC4_has_attribute___noclone__             1
# define __GCC4_has_attribute___nonstring__           0
# define __GCC4_has_attribute___no_sanitize_address__ (__GNUC_MINOR__ >= 8)
#endif

#ifndef __GCC4_has_attribute___copy__
# define __GCC4_has_attribute___copy__                0
#endif

#if __has_attribute(__copy__)
# define __copy(symbol)                 __attribute__((__copy__(symbol)))
#else
# define __copy(symbol)
#endif

#undef module_init
#define module_init(initfn)                                             \
	static int __init __init_backport(void)                         \
	{                                                               \
		backport_dependency_symbol();                           \
		return initfn();                                        \
	}                                                               \
	int init_module(void)  __copy(initfn)  __attribute__((alias("__init_backport")));


/*
 * Each compat file represents compatibility code for new kernel
 * code introduced for *that* kernel revision.
 */

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3,9,0))
#include <linux/compat-3.10.h>
#include <linux/compat-3.11.h>
#include <linux/compat-3.12.h>
#include <linux/compat-3.13.h>
#include <linux/compat-3.14.h>
#include <linux/compat-3.15.h>
#include <linux/compat-3.16.h>
#include <linux/compat-3.17.h>
#include <linux/compat-4.0.h>
#include <linux/compat-4.1.h>
#include <linux/compat-4.10.h>
#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(3,9,0) */


#ifndef HAVE_ELFCOREHDR_ADDR_EXPORTED
#define elfcorehdr_addr LINUX_BACKPORT(elfcorehdr_addr)
#endif

#endif /* LINUX_26_COMPAT_H */
