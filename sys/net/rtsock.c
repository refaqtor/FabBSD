/*	$OpenBSD: rtsock.c,v 1.75 2008/08/01 05:08:08 henning Exp $	*/
/*	$NetBSD: rtsock.c,v 1.18 1996/03/29 00:32:10 cgd Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1988, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)rtsock.c	8.6 (Berkeley) 2/11/95
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>

#include <uvm/uvm_extern.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>
#include <net/raw_cb.h>

#include <sys/stdarg.h>

struct sockaddr		route_dst = { 2, PF_ROUTE, };
struct sockaddr		route_src = { 2, PF_ROUTE, };
struct sockproto	route_proto = { PF_ROUTE, };

struct walkarg {
	int	w_op, w_arg, w_given, w_needed, w_tmemsize;
	caddr_t	w_where, w_tmem;
};

struct mbuf	*rt_msg1(int, struct rt_addrinfo *);
int		 rt_msg2(int, int, struct rt_addrinfo *, caddr_t,
		     struct walkarg *);
void		 rt_xaddrs(caddr_t, caddr_t, struct rt_addrinfo *);
#ifndef SMALL_KERNEL
struct rt_msghdr *rtmsg_3to4(struct mbuf *, int *);
#endif

/* Sleazy use of local variables throughout file, warning!!!! */
#define dst	info.rti_info[RTAX_DST]
#define gate	info.rti_info[RTAX_GATEWAY]
#define netmask	info.rti_info[RTAX_NETMASK]
#define genmask	info.rti_info[RTAX_GENMASK]
#define ifpaddr	info.rti_info[RTAX_IFP]
#define ifaaddr	info.rti_info[RTAX_IFA]
#define brdaddr	info.rti_info[RTAX_BRD]

int
route_usrreq(struct socket *so, int req, struct mbuf *m, struct mbuf *nam,
    struct mbuf *control, struct proc *p)
{
	int		 error = 0;
	struct rawcb	*rp = sotorawcb(so);
	int		 s;

	if (req == PRU_ATTACH) {
		rp = malloc(sizeof(*rp), M_PCB, M_WAITOK|M_ZERO);
		so->so_pcb = rp;
	}
	if (req == PRU_DETACH && rp) {
		int af = rp->rcb_proto.sp_protocol;
		if (af == AF_INET)
			route_cb.ip_count--;
		else if (af == AF_INET6)
			route_cb.ip6_count--;
		route_cb.any_count--;
	}
	s = splsoftnet();
	/*
	 * Don't call raw_usrreq() in the attach case, because
	 * we want to allow non-privileged processes to listen on
	 * and send "safe" commands to the routing socket.
	 */
	if (req == PRU_ATTACH) {
		if (curproc == 0)
			error = EACCES;
		else
			error = raw_attach(so, (int)(long)nam);
	} else
		error = raw_usrreq(so, req, m, nam, control, p);

	rp = sotorawcb(so);
	if (req == PRU_ATTACH && rp) {
		int af = rp->rcb_proto.sp_protocol;
		if (error) {
			free(rp, M_PCB);
			splx(s);
			return (error);
		}
		if (af == AF_INET)
			route_cb.ip_count++;
		else if (af == AF_INET6)
			route_cb.ip6_count++;
		rp->rcb_faddr = &route_src;
		route_cb.any_count++;
		soisconnected(so);
		so->so_options |= SO_USELOOPBACK;
	}
	splx(s);
	return (error);
}

int
route_output(struct mbuf *m, ...)
{
	struct rt_msghdr	*rtm = NULL;
	struct radix_node	*rn = NULL;
	struct rtentry		*rt = NULL;
	struct rtentry		*saved_nrt = NULL;
	struct radix_node_head	*rnh;
	struct rt_addrinfo	 info;
	int			 len, error = 0;
	struct ifnet		*ifp = NULL;
	struct ifaddr		*ifa = NULL;
	struct socket		*so;
	struct rawcb		*rp = NULL;
	struct sockaddr_rtlabel	 sa_rt;
	const char		*label;
	va_list			 ap;
	u_int			 tableid;
	u_int8_t		 prio;

	va_start(ap, m);
	so = va_arg(ap, struct socket *);
	va_end(ap);

	dst = NULL;	/* for error handling (goto flush) */
	if (m == 0 || ((m->m_len < sizeof(int32_t)) &&
	    (m = m_pullup(m, sizeof(int32_t))) == 0))
		return (ENOBUFS);
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("route_output");
	len = m->m_pkthdr.len;
	if (len < offsetof(struct rt_msghdr, rtm_type) + 1 ||
	    len != mtod(m, struct rt_msghdr *)->rtm_msglen) {
		error = EINVAL;
		goto flush;
	}
	switch (mtod(m, struct rt_msghdr *)->rtm_version) {
	case RTM_VERSION:
		if (len < sizeof(struct rt_msghdr)) {
			error = EINVAL;
			goto flush;
		}
		R_Malloc(rtm, struct rt_msghdr *, len);
		if (rtm == 0) {
			error = ENOBUFS;
			goto flush;
		}
		m_copydata(m, 0, len, (caddr_t)rtm);
		break;
#ifndef SMALL_KERNEL
	case RTM_OVERSION:
		if (len < sizeof(struct rt_omsghdr)) {
			error = EINVAL;
			goto flush;
		}
		rtm = rtmsg_3to4(m, &len);
		if (rtm == 0) {
			error = ENOBUFS;
			goto flush;
		}
		break;
#endif
	default:
		error = EPROTONOSUPPORT;
		goto flush;
	}
	rtm->rtm_pid = curproc->p_pid;
	if (rtm->rtm_hdrlen == 0)	/* old client */
		rtm->rtm_hdrlen = sizeof(struct rt_msghdr);
	if (len < rtm->rtm_hdrlen) {
		error = EINVAL;
		goto flush;
	}

	tableid = rtm->rtm_tableid;
	if (!rtable_exists(tableid)) {
		if (rtm->rtm_type == RTM_ADD) {
			if (rtable_add(tableid)) {
				error = EINVAL;
				goto flush;
			}
		} else {
			error = EINVAL;
			goto flush;
		}
	}

	if (rtm->rtm_priority != 0) {
		if (rtm->rtm_priority > RTP_MAX) {
			error = EINVAL;
			goto flush;
		}
		prio = rtm->rtm_priority;
	} else if (rtm->rtm_type != RTM_ADD)
		prio = RTP_ANY;
	else if (rtm->rtm_flags & RTF_STATIC)
		prio = RTP_STATIC;
	else
		prio = RTP_DEFAULT;

	/* XXX hack for 4.4-release */
	prio = RTP_DEFAULT;

	bzero(&info, sizeof(info));
	info.rti_addrs = rtm->rtm_addrs;
	rt_xaddrs(rtm->rtm_hdrlen + (caddr_t)rtm, len + (caddr_t)rtm, &info);
	info.rti_flags = rtm->rtm_flags;
	if (dst == 0 || dst->sa_family >= AF_MAX ||
	    (gate != 0 && gate->sa_family >= AF_MAX)) {
		error = EINVAL;
		goto flush;
	}
	if (genmask) {
		struct radix_node	*t;
		t = rn_addmask(genmask, 0, 1);
		if (t && genmask->sa_len >=
		    ((struct sockaddr *)t->rn_key)->sa_len &&
		    Bcmp((caddr_t *)genmask + 1, (caddr_t *)t->rn_key + 1,
		    ((struct sockaddr *)t->rn_key)->sa_len) - 1)
			genmask = (struct sockaddr *)(t->rn_key);
		else {
			error = ENOBUFS;
			goto flush;
		}
	}

	/*
	 * Verify that the caller has the appropriate privilege; RTM_GET
	 * is the only operation the non-superuser is allowed.
	 */
	if (rtm->rtm_type != RTM_GET && suser(curproc, 0) != 0) {
		error = EACCES;
		goto flush;
	}

	switch (rtm->rtm_type) {
	case RTM_ADD:
		if (gate == 0) {
			error = EINVAL;
			goto flush;
		}
		error = rtrequest1(rtm->rtm_type, &info, prio, &saved_nrt,
		    tableid);
		if (error == 0 && saved_nrt) {
			rt_setmetrics(rtm->rtm_inits, &rtm->rtm_rmx,
			    &saved_nrt->rt_rmx);
			saved_nrt->rt_refcnt--;
			saved_nrt->rt_genmask = genmask;
			rtm->rtm_index = saved_nrt->rt_ifp->if_index;
		}
		break;
	case RTM_DELETE:
		error = rtrequest1(rtm->rtm_type, &info, prio, &saved_nrt,
		    tableid);
		if (error == 0) {
			(rt = saved_nrt)->rt_refcnt++;
			goto report;
		}
		break;
	case RTM_GET:
	case RTM_CHANGE:
	case RTM_LOCK:
		if ((rnh = rt_gettable(dst->sa_family, tableid)) == NULL) {
			error = EAFNOSUPPORT;
			goto flush;
		}
		rn = rt_lookup(dst, netmask, tableid);
		if (rn == NULL || (rn->rn_flags & RNF_ROOT) != 0) {
			error = ESRCH;
			goto flush;
		}
		rt = (struct rtentry *)rn;
#ifndef SMALL_KERNEL
		/*
		 * for RTM_CHANGE/LOCK, if we got multipath routes,
		 * we require users to specify a matching RTAX_GATEWAY.
		 *
		 * for RTM_GET, gate is optional even with multipath.
		 * if gate == NULL the first match is returned.
		 * (no need to call rt_mpath_matchgate if gate == NULL)
		 */
		if (rn_mpath_capable(rnh)) {
			/* first find correct priority bucket */
			rn = rn_mpath_prio(rn, prio);
			rt = (struct rtentry *)rn;
			if (prio != RTP_ANY && rt->rt_priority != prio) {
				error = ESRCH;
				goto flush;
			}

			/* if multipath routes */
			if (rn_mpath_next(rn)) {
				if (gate)
					rt = rt_mpath_matchgate(rt, gate, prio);
				else if (rtm->rtm_type != RTM_GET)
					/*
					 * only RTM_GET may use an empty gate
					 * on multipath ...
					 */
					rt = NULL;
			} else if (gate && (rtm->rtm_type == RTM_GET ||
			    rtm->rtm_type == RTM_LOCK))
				/*
				 * ... but if a gate is specified RTM_GET
				 * and RTM_LOCK must match the gate no matter
				 * what.
				 */
				rt = rt_mpath_matchgate(rt, gate, prio);

			if (!rt) {
				error = ESRCH;
				goto flush;
			}
			rn = (struct radix_node *)rt;
		}
#endif
		rt->rt_refcnt++;

		/*
		 * RTM_CHANGE/LOCK need a perfect match, rn_lookup()
		 * returns a perfect match in case a netmask is specified.
		 * For host routes only a longest prefix match is returned
		 * so it is necessary to compare the existence of the netmaks.
		 * If both have a netmask rn_lookup() did a perfect match and
		 * if none of them have a netmask both are host routes which is
		 * also a perfect match.
		 */
		if (rtm->rtm_type != RTM_GET && !rt_mask(rt) != !netmask) {
				error = ESRCH;
				goto flush;
		}

		switch (rtm->rtm_type) {
		case RTM_GET:
report:
			dst = rt_key(rt);
			gate = rt->rt_gateway;
			netmask = rt_mask(rt);
			genmask = rt->rt_genmask;

			if (rt->rt_labelid) {
				bzero(&sa_rt, sizeof(sa_rt));
				sa_rt.sr_len = sizeof(sa_rt);
				label = rtlabel_id2name(rt->rt_labelid);
				if (label != NULL)
					strlcpy(sa_rt.sr_label, label,
					    sizeof(sa_rt.sr_label));
				info.rti_info[RTAX_LABEL] =
				    (struct sockaddr *)&sa_rt;
			}

			ifpaddr = 0;
			ifaaddr = 0;
			if (rtm->rtm_addrs & (RTA_IFP | RTA_IFA) &&
			    (ifp = rt->rt_ifp) != NULL) {
				ifpaddr =
				    TAILQ_FIRST(&ifp->if_addrlist)->ifa_addr;
				ifaaddr = rt->rt_ifa->ifa_addr;
				if (ifp->if_flags & IFF_POINTOPOINT)
					brdaddr = rt->rt_ifa->ifa_dstaddr;
				else
					brdaddr = 0;
				rtm->rtm_index = ifp->if_index;
			}
			len = rt_msg2(rtm->rtm_type, RTM_VERSION, &info, NULL,
			    NULL);
			if (len > rtm->rtm_msglen) {
				struct rt_msghdr	*new_rtm;
				R_Malloc(new_rtm, struct rt_msghdr *, len);
				if (new_rtm == 0) {
					error = ENOBUFS;
					goto flush;
				}
				Bcopy(rtm, new_rtm, rtm->rtm_msglen);
				Free(rtm); rtm = new_rtm;
			}
			rt_msg2(rtm->rtm_type, RTM_VERSION, &info, (caddr_t)rtm,
			    NULL);
			rtm->rtm_flags = rt->rt_flags;
			rtm->rtm_use = 0;
			rtm->rtm_priority = rt->rt_priority;
			rt_getmetrics(&rt->rt_rmx, &rtm->rtm_rmx);
			rtm->rtm_addrs = info.rti_addrs;
			break;

		case RTM_CHANGE:
			/*
			 * new gateway could require new ifaddr, ifp;
			 * flags may also be different; ifp may be specified
			 * by ll sockaddr when protocol address is ambiguous
			 */
			if ((error = rt_getifa(&info)) != 0)
				goto flush;
			if (gate && rt_setgate(rt, rt_key(rt), gate, tableid)) {
				error = EDQUOT;
				goto flush;
			}
			if (ifpaddr && (ifa = ifa_ifwithnet(ifpaddr)) &&
			    (ifp = ifa->ifa_ifp) && (ifaaddr || gate))
				ifa = ifaof_ifpforaddr(ifaaddr ? ifaaddr : gate,
							ifp);
			else if ((ifaaddr && (ifa = ifa_ifwithaddr(ifaaddr))) ||
			    (gate && (ifa = ifa_ifwithroute(rt->rt_flags,
			    rt_key(rt), gate))))
				ifp = ifa->ifa_ifp;
			if (ifa) {
				struct ifaddr *oifa = rt->rt_ifa;
				if (oifa != ifa) {
				    if (oifa && oifa->ifa_rtrequest)
					oifa->ifa_rtrequest(RTM_DELETE, rt,
					    &info);
				    IFAFREE(rt->rt_ifa);
				    rt->rt_ifa = ifa;
				    ifa->ifa_refcnt++;
				    rt->rt_ifp = ifp;
				}
			}

			/* XXX Hack to allow some flags to be toggled */
			if (rtm->rtm_fmask & RTF_FMASK)
				rt->rt_flags = (rt->rt_flags &
				    ~rtm->rtm_fmask) |
				    (rtm->rtm_flags & rtm->rtm_fmask);

			rt_setmetrics(rtm->rtm_inits, &rtm->rtm_rmx,
			    &rt->rt_rmx);
			rtm->rtm_index = rt->rt_ifp->if_index;
			if (rt->rt_ifa && rt->rt_ifa->ifa_rtrequest)
				rt->rt_ifa->ifa_rtrequest(RTM_ADD, rt, &info);
			if (genmask)
				rt->rt_genmask = genmask;
			if (info.rti_info[RTAX_LABEL] != NULL) {
				char *rtlabel = ((struct sockaddr_rtlabel *)
				    info.rti_info[RTAX_LABEL])->sr_label;
				rtlabel_unref(rt->rt_labelid);
				rt->rt_labelid =
				    rtlabel_name2id(rtlabel);
			}
			if_group_routechange(dst, netmask);
			/* FALLTHROUGH */
		case RTM_LOCK:
			rt->rt_rmx.rmx_locks &= ~(rtm->rtm_inits);
			rt->rt_rmx.rmx_locks |=
			    (rtm->rtm_inits & rtm->rtm_rmx.rmx_locks);
			break;
		}
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

flush:
	if (rtm) {
		if (error)
			rtm->rtm_errno = error;
		else 
			rtm->rtm_flags |= RTF_DONE;
	}
	if (rt)
		rtfree(rt);

	/*
	 * Check to see if we don't want our own messages.
	 */
	if (!(so->so_options & SO_USELOOPBACK)) {
		if (route_cb.any_count <= 1) {
			if (rtm)
				Free(rtm);
			m_freem(m);
			return (error);
		}
		/* There is another listener, so construct message */
		rp = sotorawcb(so);
	}
	if (rp)
		rp->rcb_proto.sp_family = 0; /* Avoid us */
	if (dst)
		route_proto.sp_protocol = dst->sa_family;
	if (rtm) {
		m_copyback(m, 0, rtm->rtm_msglen, rtm);
		if (m->m_pkthdr.len < rtm->rtm_msglen) {
			m_freem(m);
			m = NULL;
		} else if (m->m_pkthdr.len > rtm->rtm_msglen)
			m_adj(m, rtm->rtm_msglen - m->m_pkthdr.len);
		Free(rtm);
	}
	if (m)
		raw_input(m, &route_proto, &route_src, &route_dst);
	if (rp)
		rp->rcb_proto.sp_family = PF_ROUTE;

	return (error);
}

void
rt_setmetrics(u_long which, struct rt_metrics *in, struct rt_kmetrics *out)
{
	if (which & RTV_MTU)
		out->rmx_mtu = in->rmx_mtu;
	if (which & RTV_EXPIRE)
		out->rmx_expire = in->rmx_expire;
	/* RTV_PRIORITY handled befor */
}

void
rt_getmetrics(struct rt_kmetrics *in, struct rt_metrics *out)
{
	bzero(out, sizeof(*out));
	out->rmx_locks = in->rmx_locks;
	out->rmx_mtu = in->rmx_mtu;
	out->rmx_expire = in->rmx_expire;
	out->rmx_pksent = in->rmx_pksent;
}

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

void
rt_xaddrs(caddr_t cp, caddr_t cplim, struct rt_addrinfo *rtinfo)
{
	struct sockaddr	*sa;
	int		 i;

	bzero(rtinfo->rti_info, sizeof(rtinfo->rti_info));
	for (i = 0; (i < RTAX_MAX) && (cp < cplim); i++) {
		if ((rtinfo->rti_addrs & (1 << i)) == 0)
			continue;
		rtinfo->rti_info[i] = sa = (struct sockaddr *)cp;
		ADVANCE(cp, sa);
	}
}

struct mbuf *
rt_msg1(int type, struct rt_addrinfo *rtinfo)
{
	struct rt_msghdr	*rtm;
	struct mbuf		*m;
	int			 i;
	struct sockaddr		*sa;
	int			 len, dlen, hlen;

	switch (type) {
	case RTM_DELADDR:
	case RTM_NEWADDR:
		len = sizeof(struct ifa_msghdr);
		break;
	case RTM_IFINFO:
		len = sizeof(struct if_msghdr);
		break;
	case RTM_IFANNOUNCE:
		len = sizeof(struct if_announcemsghdr);
		break;
	default:
		len = sizeof(struct rt_msghdr);
		break;
	}
	if (len > MCLBYTES)
		panic("rt_msg1");
	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m && len > MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_free(m);
			m = NULL;
		}
	}
	if (m == 0)
		return (m);
	m->m_pkthdr.len = m->m_len = hlen = len;
	m->m_pkthdr.rcvif = NULL;
	rtm = mtod(m, struct rt_msghdr *);
	bzero(rtm, len);
	for (i = 0; i < RTAX_MAX; i++) {
		if ((sa = rtinfo->rti_info[i]) == NULL)
			continue;
		rtinfo->rti_addrs |= (1 << i);
		dlen = ROUNDUP(sa->sa_len);
		m_copyback(m, len, dlen, sa);
		len += dlen;
	}
	if (m->m_pkthdr.len != len) {
		m_freem(m);
		return (NULL);
	}
	rtm->rtm_msglen = len;
	rtm->rtm_hdrlen = hlen;
	rtm->rtm_version = RTM_VERSION;
	rtm->rtm_type = type;
	return (m);
}

int
rt_msg2(int type, int vers, struct rt_addrinfo *rtinfo, caddr_t cp,
    struct walkarg *w)
{
	int		i;
	int		len, dlen, hlen, second_time = 0;
	caddr_t		cp0;

	rtinfo->rti_addrs = 0;
again:
	switch (type) {
	case RTM_DELADDR:
	case RTM_NEWADDR:
#ifndef SMALL_KERNEL
		if (vers == RTM_OVERSION)
			len = sizeof(struct ifa_omsghdr);
		else
#endif
			len = sizeof(struct ifa_msghdr);
		break;
	case RTM_IFINFO:
#ifndef SMALL_KERNEL
		if (vers == RTM_OVERSION)
			len = sizeof(struct if_omsghdr);
		else
#endif
			len = sizeof(struct if_msghdr);
		break;
	default:
#ifndef SMALL_KERNEL
		if (vers == RTM_OVERSION)
			len = sizeof(struct rt_omsghdr);
		else
#endif
			len = sizeof(struct rt_msghdr);
		break;
	}
	hlen = len;
	if ((cp0 = cp) != NULL)
		cp += len;
	for (i = 0; i < RTAX_MAX; i++) {
		struct sockaddr *sa;

		if ((sa = rtinfo->rti_info[i]) == 0)
			continue;
		rtinfo->rti_addrs |= (1 << i);
		dlen = ROUNDUP(sa->sa_len);
		if (cp) {
			bcopy(sa, cp, (size_t)dlen);
			cp += dlen;
		}
		len += dlen;
	}
	/* align message length to the next natural boundary */
	len = ALIGN(len);
	if (cp == 0 && w != NULL && !second_time) {
		struct walkarg *rw = w;

		rw->w_needed += len;
		if (rw->w_needed <= 0 && rw->w_where) {
			if (rw->w_tmemsize < len) {
				if (rw->w_tmem)
					free(rw->w_tmem, M_RTABLE);
				rw->w_tmem = malloc(len, M_RTABLE, M_NOWAIT);
				if (rw->w_tmem)
					rw->w_tmemsize = len;
			}
			if (rw->w_tmem) {
				cp = rw->w_tmem;
				second_time = 1;
				goto again;
			} else
				rw->w_where = 0;
		}
	}
	if (cp && w)		/* clear the message header */
		bzero(cp0, hlen);

	if (cp && vers != RTM_OVERSION) {
		struct rt_msghdr *rtm = (struct rt_msghdr *)cp0;

		rtm->rtm_version = RTM_VERSION;
		rtm->rtm_type = type;
		rtm->rtm_msglen = len;
		rtm->rtm_hdrlen = hlen;
	}
#ifndef SMALL_KERNEL
	if (cp && vers == RTM_OVERSION) {
		struct rt_omsghdr *rtm = (struct rt_omsghdr *)cp0;

		rtm->rtm_version = RTM_OVERSION;
		rtm->rtm_type = type;
		rtm->rtm_msglen = len;
	}
#endif
	return (len);
}

/*
 * This routine is called to generate a message from the routing
 * socket indicating that a redirect has occurred, a routing lookup
 * has failed, or that a protocol has detected timeouts to a particular
 * destination.
 */
void
rt_missmsg(int type, struct rt_addrinfo *rtinfo, int flags,
    struct ifnet *ifp, int error, u_int tableid)
{
	struct rt_msghdr	*rtm;
	struct mbuf		*m;
	struct sockaddr		*sa = rtinfo->rti_info[RTAX_DST];

	if (route_cb.any_count == 0)
		return;
	m = rt_msg1(type, rtinfo);
	if (m == 0)
		return;
	rtm = mtod(m, struct rt_msghdr *);
	rtm->rtm_flags = RTF_DONE | flags;
	rtm->rtm_errno = error;
	rtm->rtm_tableid = tableid;
	rtm->rtm_addrs = rtinfo->rti_addrs;
	if (ifp != NULL)
		rtm->rtm_index = ifp->if_index;
	if (sa == NULL)
		route_proto.sp_protocol = 0;
	else
		route_proto.sp_protocol = sa->sa_family;
	raw_input(m, &route_proto, &route_src, &route_dst);
}

/*
 * This routine is called to generate a message from the routing
 * socket indicating that the status of a network interface has changed.
 */
void
rt_ifmsg(struct ifnet *ifp)
{
	struct if_msghdr	*ifm;
	struct mbuf		*m;
	struct rt_addrinfo	 info;

	if (route_cb.any_count == 0)
		return;
	bzero(&info, sizeof(info));
	m = rt_msg1(RTM_IFINFO, &info);
	if (m == 0)
		return;
	ifm = mtod(m, struct if_msghdr *);
	ifm->ifm_index = ifp->if_index;
	ifm->ifm_flags = ifp->if_flags;
	ifm->ifm_data = ifp->if_data;
	ifm->ifm_addrs = 0;
	route_proto.sp_protocol = 0;
	raw_input(m, &route_proto, &route_src, &route_dst);
}

/*
 * This is called to generate messages from the routing socket
 * indicating a network interface has had addresses associated with it.
 * if we ever reverse the logic and replace messages TO the routing
 * socket indicate a request to configure interfaces, then it will
 * be unnecessary as the routing socket will automatically generate
 * copies of it.
 */
void
rt_newaddrmsg(int cmd, struct ifaddr *ifa, int error, struct rtentry *rt)
{
	struct rt_addrinfo	 info;
	struct sockaddr		*sa = NULL;
	int			 pass;
	struct mbuf		*m = NULL;
	struct ifnet		*ifp = ifa->ifa_ifp;

	if (route_cb.any_count == 0)
		return;
	for (pass = 1; pass < 3; pass++) {
		bzero(&info, sizeof(info));
		if ((cmd == RTM_ADD && pass == 1) ||
		    (cmd == RTM_DELETE && pass == 2)) {
			struct ifa_msghdr	*ifam;
			int			 ncmd;

			if (cmd == RTM_ADD)
				ncmd = RTM_NEWADDR;
			else
				ncmd = RTM_DELADDR;

			ifaaddr = sa = ifa->ifa_addr;
			ifpaddr = TAILQ_FIRST(&ifp->if_addrlist)->ifa_addr;
			netmask = ifa->ifa_netmask;
			brdaddr = ifa->ifa_dstaddr;
			if ((m = rt_msg1(ncmd, &info)) == NULL)
				continue;
			ifam = mtod(m, struct ifa_msghdr *);
			ifam->ifam_index = ifp->if_index;
			ifam->ifam_metric = ifa->ifa_metric;
			ifam->ifam_flags = ifa->ifa_flags;
			ifam->ifam_addrs = info.rti_addrs;
		}
		if ((cmd == RTM_ADD && pass == 2) ||
		    (cmd == RTM_DELETE && pass == 1)) {
			struct rt_msghdr *rtm;
			
			if (rt == 0)
				continue;
			netmask = rt_mask(rt);
			dst = sa = rt_key(rt);
			gate = rt->rt_gateway;
			if ((m = rt_msg1(cmd, &info)) == NULL)
				continue;
			rtm = mtod(m, struct rt_msghdr *);
			rtm->rtm_index = ifp->if_index;
			rtm->rtm_flags |= rt->rt_flags;
			rtm->rtm_errno = error;
			rtm->rtm_addrs = info.rti_addrs;
		}
		if (sa == NULL)
			route_proto.sp_protocol = 0;
		else
			route_proto.sp_protocol = sa->sa_family;
		raw_input(m, &route_proto, &route_src, &route_dst);
	}
}

/*
 * This is called to generate routing socket messages indicating
 * network interface arrival and departure.
 */
void
rt_ifannouncemsg(struct ifnet *ifp, int what)
{
	struct if_announcemsghdr	*ifan;
	struct mbuf			*m;
	struct rt_addrinfo		 info;

	if (route_cb.any_count == 0)
		return;
	bzero(&info, sizeof(info));
	m = rt_msg1(RTM_IFANNOUNCE, &info);
	if (m == 0)
		return;
	ifan = mtod(m, struct if_announcemsghdr *);
	ifan->ifan_index = ifp->if_index;
	strlcpy(ifan->ifan_name, ifp->if_xname, sizeof(ifan->ifan_name));
	ifan->ifan_what = what;
	route_proto.sp_protocol = 0;
	raw_input(m, &route_proto, &route_src, &route_dst);
}

/*
 * This is used in dumping the kernel table via sysctl().
 */
int
sysctl_dumpentry(struct radix_node *rn, void *v)
{
	struct walkarg		*w = v;
	struct rtentry		*rt = (struct rtentry *)rn;
	int			 error = 0, size;
	struct rt_addrinfo	 info;
	struct sockaddr_rtlabel	 sa_rt;
	const char		*label;

	if (w->w_op == NET_RT_FLAGS && !(rt->rt_flags & w->w_arg))
		return 0;
	bzero(&info, sizeof(info));
	dst = rt_key(rt);
	gate = rt->rt_gateway;
	netmask = rt_mask(rt);
	genmask = rt->rt_genmask;
	if (rt->rt_ifp) {
		ifpaddr = TAILQ_FIRST(&rt->rt_ifp->if_addrlist)->ifa_addr;
		ifaaddr = rt->rt_ifa->ifa_addr;
		if (rt->rt_ifp->if_flags & IFF_POINTOPOINT)
			brdaddr = rt->rt_ifa->ifa_dstaddr;
	}
	if (rt->rt_labelid) {
		bzero(&sa_rt, sizeof(sa_rt));
		sa_rt.sr_len = sizeof(sa_rt);
		label = rtlabel_id2name(rt->rt_labelid);
		if (label != NULL) {
			strlcpy(sa_rt.sr_label, label,
			    sizeof(sa_rt.sr_label));
			info.rti_info[RTAX_LABEL] =
			    (struct sockaddr *)&sa_rt;
		}
	}

	size = rt_msg2(RTM_GET, RTM_VERSION, &info, NULL, w);
	if (w->w_where && w->w_tmem && w->w_needed <= 0) {
		struct rt_msghdr *rtm = (struct rt_msghdr *)w->w_tmem;

		rtm->rtm_flags = rt->rt_flags;
		rtm->rtm_priority = rt->rt_priority;
		rt_getmetrics(&rt->rt_rmx, &rtm->rtm_rmx);
		rtm->rtm_rmx.rmx_refcnt = rt->rt_refcnt;
		rtm->rtm_index = rt->rt_ifp->if_index;
		rtm->rtm_addrs = info.rti_addrs;
		if ((error = copyout(rtm, w->w_where, size)) != 0)
			w->w_where = NULL;
		else
			w->w_where += size;
	}
#ifndef SMALL_KERNEL
	size = rt_msg2(RTM_GET, RTM_OVERSION, &info, NULL, w);
	if (w->w_where && w->w_tmem && w->w_needed <= 0) {
		struct rt_omsghdr *rtm = (struct rt_omsghdr *)w->w_tmem;

		rtm->rtm_flags = rt->rt_flags;
		rtm->rtm_rmx.rmx_locks = rt->rt_rmx.rmx_locks;
		rtm->rtm_rmx.rmx_mtu = rt->rt_rmx.rmx_mtu;
		rtm->rtm_index = rt->rt_ifp->if_index;
		rtm->rtm_addrs = info.rti_addrs;
		if ((error = copyout(rtm, w->w_where, size)) != 0)
			w->w_where = NULL;
		else
			w->w_where += size;
	}
#endif
	return (error);
}

int
sysctl_iflist(int af, struct walkarg *w)
{
	struct ifnet		*ifp;
	struct ifaddr		*ifa;
	struct rt_addrinfo	 info;
	int			 len, error = 0;

	bzero(&info, sizeof(info));
	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if (w->w_arg && w->w_arg != ifp->if_index)
			continue;
		ifa = TAILQ_FIRST(&ifp->if_addrlist);
		if (!ifa)
			continue;
		ifpaddr = ifa->ifa_addr;
		len = rt_msg2(RTM_IFINFO, RTM_VERSION, &info, 0, w);
		if (w->w_where && w->w_tmem && w->w_needed <= 0) {
			struct if_msghdr *ifm;

			ifm = (struct if_msghdr *)w->w_tmem;
			ifm->ifm_index = ifp->if_index;
			ifm->ifm_flags = ifp->if_flags;
			ifm->ifm_data = ifp->if_data;
			ifm->ifm_addrs = info.rti_addrs;
			error = copyout(ifm, w->w_where, len);
			if (error)
				return (error);
			w->w_where += len;
		}
#ifndef SMALL_KERNEL
		len = rt_msg2(RTM_IFINFO, RTM_OVERSION, &info, 0, w);
		if (w->w_where && w->w_tmem && w->w_needed <= 0) {
			struct if_omsghdr *ifm;

			ifm = (struct if_omsghdr *)w->w_tmem;
			ifm->ifm_index = ifp->if_index;
			ifm->ifm_flags = ifp->if_flags;
			/* just init the most important types of if_data */
			ifm->ifm_data.ifi_type = ifp->if_data.ifi_type;
			ifm->ifm_data.ifi_addrlen = ifp->if_data.ifi_addrlen;
			ifm->ifm_data.ifi_hdrlen = ifp->if_data.ifi_hdrlen;
			ifm->ifm_data.ifi_link_state =
			    ifp->if_data.ifi_link_state;
			ifm->ifm_data.ifi_mtu = ifp->if_data.ifi_mtu;
			ifm->ifm_data.ifi_metric = ifp->if_data.ifi_metric;
			if (ifp->if_data.ifi_baudrate > ULONG_MAX)
				ifm->ifm_data.ifi_baudrate = ULONG_MAX;
			else
				ifm->ifm_data.ifi_baudrate =
				    ifp->if_data.ifi_baudrate;

			ifm->ifm_addrs = info.rti_addrs;
			error = copyout(ifm, w->w_where, len);
			if (error)
				return (error);
			w->w_where += len;
		}
#endif
		ifpaddr = 0;
		while ((ifa = TAILQ_NEXT(ifa, ifa_list)) !=
		    TAILQ_END(&ifp->if_addrlist)) {
			if (af && af != ifa->ifa_addr->sa_family)
				continue;
			ifaaddr = ifa->ifa_addr;
			netmask = ifa->ifa_netmask;
			brdaddr = ifa->ifa_dstaddr;
			len = rt_msg2(RTM_NEWADDR, RTM_VERSION, &info, 0, w);
			if (w->w_where && w->w_tmem && w->w_needed <= 0) {
				struct ifa_msghdr *ifam;

				ifam = (struct ifa_msghdr *)w->w_tmem;
				ifam->ifam_index = ifa->ifa_ifp->if_index;
				ifam->ifam_flags = ifa->ifa_flags;
				ifam->ifam_metric = ifa->ifa_metric;
				ifam->ifam_addrs = info.rti_addrs;
				error = copyout(w->w_tmem, w->w_where, len);
				if (error)
					return (error);
				w->w_where += len;
			}
#ifndef SMALL_KERNEL
			len = rt_msg2(RTM_NEWADDR, RTM_OVERSION, &info, 0, w);
			if (w->w_where && w->w_tmem && w->w_needed <= 0) {
				struct ifa_omsghdr *ifam;

				ifam = (struct ifa_omsghdr *)w->w_tmem;
				ifam->ifam_index = ifa->ifa_ifp->if_index;
				ifam->ifam_flags = ifa->ifa_flags;
				ifam->ifam_metric = ifa->ifa_metric;
				ifam->ifam_addrs = info.rti_addrs;
				error = copyout(w->w_tmem, w->w_where, len);
				if (error)
					return (error);
				w->w_where += len;
			}
#endif
		}
		ifaaddr = netmask = brdaddr = 0;
	}
	return (0);
}

int
sysctl_rtable(int *name, u_int namelen, void *where, size_t *given, void *new,
    size_t newlen)
{
	struct radix_node_head	*rnh;
	int			 i, s, error = EINVAL;
	u_char  		 af;
	struct walkarg		 w;
	u_int			 tableid = 0;

	if (new)
		return (EPERM);
	if (namelen < 3 || namelen > 4)
		return (EINVAL);
	af = name[0];
	bzero(&w, sizeof(w));
	w.w_where = where;
	w.w_given = *given;
	w.w_needed = 0 - w.w_given;
	w.w_op = name[1];
	w.w_arg = name[2];

	if (namelen == 4) {
		tableid = name[3];
		if (!rtable_exists(tableid))
			return (EINVAL);
	}

	s = splsoftnet();
	switch (w.w_op) {

	case NET_RT_DUMP:
	case NET_RT_FLAGS:
		for (i = 1; i <= AF_MAX; i++)
			if ((rnh = rt_gettable(i, tableid)) != NULL &&
			    (af == 0 || af == i) &&
			    (error = (*rnh->rnh_walktree)(rnh,
			    sysctl_dumpentry, &w)))
				break;
		break;

	case NET_RT_IFLIST:
		error = sysctl_iflist(af, &w);
		break;

	case NET_RT_STATS:
		error = sysctl_rdstruct(where, given, new,
		    &rtstat, sizeof(rtstat));
		splx(s);
		return (error);
	}
	splx(s);
	if (w.w_tmem)
		free(w.w_tmem, M_RTABLE);
	w.w_needed += w.w_given;
	if (where) {
		*given = w.w_where - (caddr_t)where;
		if (*given < w.w_needed)
			return (ENOMEM);
	} else
		*given = (11 * w.w_needed) / 10;

	return (error);
}

#ifndef SMALL_KERNEL
struct rt_msghdr *
rtmsg_3to4(struct mbuf *m, int *len)
{
	struct rt_msghdr *rtm;
	struct rt_omsghdr *ortm;
	int slen;

	slen = *len - sizeof(struct rt_omsghdr);
	*len = sizeof(struct rt_msghdr) + slen;
	R_Malloc(rtm, struct rt_msghdr *, *len);
	if (rtm == 0)
		return (NULL);
	bzero(rtm, sizeof(struct rt_msghdr));
	ortm = mtod(m, struct rt_omsghdr *);
	rtm->rtm_msglen = sizeof(struct rt_msghdr) + slen;
	rtm->rtm_version = RTM_VERSION;
	rtm->rtm_type = ortm->rtm_type;
	rtm->rtm_hdrlen = sizeof(struct rt_msghdr);
	rtm->rtm_index = ortm->rtm_index;
	rtm->rtm_tableid = 0; /* XXX we only care about the main table */
	rtm->rtm_flags = ortm->rtm_flags;
	rtm->rtm_addrs = ortm->rtm_addrs;
	rtm->rtm_seq = ortm->rtm_seq;
	rtm->rtm_fmask = ortm->rtm_fmask;
	rtm->rtm_inits = ortm->rtm_inits;
	/* copy just the interesting stuff ignore the rest */
	rtm->rtm_rmx.rmx_locks = ortm->rtm_rmx.rmx_locks;
	rtm->rtm_rmx.rmx_mtu = ortm->rtm_rmx.rmx_mtu;

	m_copydata(m, sizeof(struct rt_omsghdr), slen,
	    ((caddr_t)rtm + sizeof(struct rt_msghdr)));

	return (rtm);
}
#endif

/*
 * Definitions of protocols supported in the ROUTE domain.
 */

extern	struct domain routedomain;		/* or at least forward */

struct protosw routesw[] = {
{ SOCK_RAW,	&routedomain,	0,		PR_ATOMIC|PR_ADDR,
  raw_input,	route_output,	raw_ctlinput,	0,
  route_usrreq,
  raw_init,	0,		0,		0,
  sysctl_rtable,
}
};

struct domain routedomain =
    { PF_ROUTE, "route", route_init, 0, 0,
      routesw, &routesw[sizeof(routesw)/sizeof(routesw[0])] };
