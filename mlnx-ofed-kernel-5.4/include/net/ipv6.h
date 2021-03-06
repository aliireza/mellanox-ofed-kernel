#ifndef _COMPAT_NET_IPV6_H
#define _COMPAT_NET_IPV6_H

/* Include the autogenerated header file */
#include "../../compat/config.h"

#include_next <net/ipv6.h>

#define ipv6_addr_copy(a, b) (*(a) = *(b))

#ifndef HAVE_IP6_DST_HOPLIMIT
#define ip6_dst_hoplimit  LINUX_BACKPORT(ip6_dst_hoplimit)
int ip6_dst_hoplimit(struct dst_entry *dst);
#endif

#ifndef HAVE_IP6_MAKE_FLOWINFO
#define IPV6_TCLASS_SHIFT	20
static inline __be32 ip6_make_flowinfo(unsigned int tclass, __be32 flowlabel)
{
	return htonl(tclass << IPV6_TCLASS_SHIFT) | flowlabel;
}
#endif

#endif /* _COMPAT_NET_IPV6_H */
