/*
 * Linux /proc/net/dev metrics cluster
 *
 * Copyright (c) 2013-2014 Red Hat.
 * Copyright (c) 1995,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "indom.h"
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <net/if.h>
#include <ctype.h>
#include "proc_net_dev.h"

static int
refresh_inet_socket()
{
    static int netfd = -1;
    if (netfd < 0)
	netfd = socket(AF_INET, SOCK_DGRAM, 0);
    return netfd;
}

static int
refresh_net_dev_ioctl(char *name, net_interface_t *netip)
{
    struct ethtool_cmd ecmd = { 0 };
    /*
     * Note:
     * Initialization of ecmd is not really needed.  If the ioctl()s
     * work, ecmd is filled in ... but valgrind (at least up to
     * version 3.9.0) does not know about the SIOCETHTOOL ioctl()
     * and thinks the use of ecmd after this call propagates
     * uninitialized data in to ioc.speed and ioc.duplex, causing
     * failures for qa/957
     * - Ken McDonell, 11 Apr 2014
     */
    struct ifreq ifr;
    int fd;

    if ((fd = refresh_inet_socket()) < 0)
	return 0;

    ecmd.cmd = ETHTOOL_GSET;
    ifr.ifr_data = (caddr_t)&ecmd;
    strncpy(ifr.ifr_name, name, IF_NAMESIZE);
    ifr.ifr_name[IF_NAMESIZE-1] = '\0';
    if (!(ioctl(fd, SIOCGIFMTU, &ifr) < 0))
	netip->ioc.mtu = ifr.ifr_mtu;

    ecmd.cmd = ETHTOOL_GSET;
    ifr.ifr_data = (caddr_t)&ecmd;
    strncpy(ifr.ifr_name, name, IF_NAMESIZE);
    ifr.ifr_name[IF_NAMESIZE-1] = '\0';
    if (!(ioctl(fd, SIOCGIFFLAGS, &ifr) < 0)) {
	netip->ioc.linkup = !!(ifr.ifr_flags & IFF_UP);
	netip->ioc.running = !!(ifr.ifr_flags & IFF_RUNNING);
    }
    /* ETHTOOL ioctl -> non-root permissions issues for old kernels */
    ecmd.cmd = ETHTOOL_GSET;
    ifr.ifr_data = (caddr_t)&ecmd;
    strncpy(ifr.ifr_name, name, IF_NAMESIZE);
    ifr.ifr_name[IF_NAMESIZE-1] = '\0';
    if (!(ioctl(fd, SIOCETHTOOL, &ifr) < 0)) {
	/*
	 * speed is defined in ethtool.h and returns the speed in
	 * Mbps, so 100 for 100Mbps, 1000 for 1Gbps, etc
	 */
	netip->ioc.speed = ecmd.speed;
	netip->ioc.duplex = ecmd.duplex + 1;
	return 0;
    }
    return -ENOSYS;	/* caller should try ioctl alternatives */
}

static void
refresh_net_ipv4_addr(char *name, net_addr_t *addr)
{
    struct ifreq ifr;
    int fd;

    if ((fd = refresh_inet_socket()) < 0)
	return;
    strncpy(ifr.ifr_name, name, IF_NAMESIZE);
    ifr.ifr_name[IF_NAMESIZE-1] = '\0';
    ifr.ifr_addr.sa_family = AF_INET;
    if (ioctl(fd, SIOCGIFADDR, &ifr) >= 0) {
	struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
	if (inet_ntop(AF_INET, &sin->sin_addr, addr->inet, INET_ADDRSTRLEN))
	    addr->has_inet = 1;
    }
}

/*
 * No ioctl support or no permissions (more likely), so we
 * fall back to grovelling about in /sys/class/net in a last
 * ditch attempt to find the ethtool interface data (duplex
 * and speed).
 */
static char *
read_oneline(const char *path, char *buffer)
{
    FILE *fp = fopen(path, "r");

    if (fp) {
	int i = fscanf(fp, "%63s", buffer);
	fclose(fp);
	if (i == 1)
	    return buffer;
    }
    return "";
}

static void
refresh_net_dev_sysfs(char *name, net_interface_t *netip)
{
    char path[MAXPATHLEN];
    char line[64];
    char *duplex;

    snprintf(path, sizeof(path), "%s/sys/class/net/%s/speed", linux_statspath, name);
    path[sizeof(path)-1] = '\0';
    netip->ioc.speed = atoi(read_oneline(path, line));

    snprintf(path, sizeof(path), "%s/sys/class/net/%s/duplex", linux_statspath, name);
    path[sizeof(path)-1] = '\0';
    duplex = read_oneline(path, line);

    if (strcmp(duplex, "full") == 0)
	netip->ioc.duplex = 2;
    else if (strcmp(duplex, "half") == 0)
	netip->ioc.duplex = 1;
    else	/* eh? */
	netip->ioc.duplex = 0;
}

static void
refresh_net_hw_addr(char *name, net_addr_t *netip)
{
    char path[MAXPATHLEN];
    char line[64];
    char *value;

    snprintf(path, sizeof(path), "%s/sys/class/net/%s/address", linux_statspath, name);
    path[sizeof(path)-1] = '\0';

    value = read_oneline(path, line);

    if (value[0] != '\0')
	netip->has_hw = 1;
    strncpy(netip->hw_addr, value, sizeof(netip->hw_addr));
    netip->hw_addr[sizeof(netip->hw_addr)-1] = '\0';
}

int
refresh_proc_net_dev(pmInDom indom)
{
    char		buf[1024];
    FILE		*fp;
    unsigned long long	llval;
    char		*p, *v;
    int			j, sts;
    net_interface_t	*netip;

    static uint64_t	gen;	/* refresh generation number */
    static uint32_t	cache_err;

    if ((fp = linux_statsfile("/proc/net/dev", buf, sizeof(buf))) == NULL)
    	return -oserror();

    if (gen == 0) {
	/*
	 * first time, reload cache from external file, and force any
	 * subsequent changes to be saved
	 */
	pmdaCacheOp(indom, PMDA_CACHE_LOAD);
    }
    gen++;

    /*
Inter-|   Receive                                                |  Transmit
 face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
    lo: 4060748   39057    0    0    0     0          0         0  4060748   39057    0    0    0     0       0          0
  eth0:       0  337614    0    0    0     0          0         0        0  267537    0    0    0 27346      62          0
     */

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if ((p = v = strchr(buf, ':')) == NULL)
	    continue;
	*p = '\0';
	for (p=buf; *p && isspace((int)*p); p++) {;}

	sts = pmdaCacheLookupName(indom, p, NULL, (void **)&netip);
	if (sts == PM_ERR_INST || (sts >= 0 && netip == NULL)) {
	    /* first time since re-loaded, else new one */
	    netip = (net_interface_t *)calloc(1, sizeof(net_interface_t));
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LIBPMDA) {
		fprintf(stderr, "refresh_proc_net_dev: initialize \"%s\"\n", p);
	    }
#endif
	}
	else if (sts < 0) {
	    if (cache_err++ < 10) {
		fprintf(stderr, "refresh_proc_net_dev: pmdaCacheLookupName(%s, %s, ...) failed: %s\n",
		    pmInDomStr(indom), p, pmErrStr(sts));
	    }
	    continue;
	}
	if (netip->last_gen != gen-1) {
	    /*
	     * rediscovered one that went away and has returned
	     *
	     * kernel counters are reset, so clear last_counters to
	     * avoid false overflows
	     */
	    for (j=0; j < PROC_DEV_COUNTERS_PER_LINE; j++) {
		netip->last_counters[j] = 0;
	    }
	}
	netip->last_gen = gen;
	if ((sts = pmdaCacheStore(indom, PMDA_CACHE_ADD, p, (void *)netip)) < 0) {
	    if (cache_err++ < 10) {
		fprintf(stderr, "refresh_proc_net_dev: pmdaCacheStore(%s, PMDA_CACHE_ADD, %s, " PRINTF_P_PFX "%p) failed: %s\n",
		    pmInDomStr(indom), p, netip, pmErrStr(sts));
	    }
	    continue;
	}

	/* Issue ioctls for remaining data, not exported through proc */
	memset(&netip->ioc, 0, sizeof(netip->ioc));
	if (refresh_net_dev_ioctl(p, netip) < 0)
	    refresh_net_dev_sysfs(p, netip);

	for (p=v, j=0; j < PROC_DEV_COUNTERS_PER_LINE; j++) {
	    for (; !isdigit((int)*p); p++) {;}
	    sscanf(p, "%llu", &llval);
	    if (llval >= netip->last_counters[j]) {
		netip->counters[j] +=
		    llval - netip->last_counters[j];
	    }
	    else {
	    	/* 32bit counter has wrapped */
		netip->counters[j] +=
		    llval + (UINT_MAX - netip->last_counters[j]);
	    }
	    netip->last_counters[j] = llval;
	    for (; !isspace((int)*p); p++) {;}
	}
    }

    pmdaCacheOp(indom, PMDA_CACHE_SAVE);

    /* success */
    fclose(fp);
    return 0;
}

static int
refresh_net_dev_ipv4_addr(pmInDom indom)
{
    int n, fd, sts, numreqs = 30;
    struct ifconf ifc;
    struct ifreq *ifr;
    net_addr_t *netip;
    static uint32_t cache_err;

    if ((fd = refresh_inet_socket()) < 0)
	return fd;

    ifc.ifc_buf = NULL;
    for (;;) {
	ifc.ifc_len = sizeof(struct ifreq) * numreqs;
	ifc.ifc_buf = realloc(ifc.ifc_buf, ifc.ifc_len);

	if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
	    free(ifc.ifc_buf);
	    return -oserror();
	}
	if (ifc.ifc_len == sizeof(struct ifreq) * numreqs) {
	    /* assume it overflowed and try again */
	    numreqs *= 2;
	    continue;
	}
	break;
    }

    for (n = 0, ifr = ifc.ifc_req;
	 n < ifc.ifc_len;
	 n += sizeof(struct ifreq), ifr++) {
	sts = pmdaCacheLookupName(indom, ifr->ifr_name, NULL, (void **)&netip);
	if (sts == PM_ERR_INST || (sts >= 0 && netip == NULL)) {
	    /* first time since re-loaded, else new one */
	    netip = (net_addr_t *)calloc(1, sizeof(net_addr_t));
	}
	else if (sts < 0) {
	    if (cache_err++ < 10) {
		fprintf(stderr, "refresh_net_dev_ipv4_addr: "
			"pmdaCacheLookupName(%s, %s, ...) failed: %s\n",
		    pmInDomStr(indom), ifr->ifr_name, pmErrStr(sts));
	    }
	    continue;
	}
	if ((sts = pmdaCacheStore(indom, PMDA_CACHE_ADD, ifr->ifr_name, (void *)netip)) < 0) {
	    if (cache_err++ < 10) {
		fprintf(stderr, "refresh_net_dev_ipv4_addr: "
			"pmdaCacheStore(%s, PMDA_CACHE_ADD, %s, "
			PRINTF_P_PFX "%p) failed: %s\n",
		    pmInDomStr(indom), ifr->ifr_name, netip, pmErrStr(sts));
	    }
	    continue;
	}

	refresh_net_ipv4_addr(ifr->ifr_name, netip);
	refresh_net_hw_addr(ifr->ifr_name, netip);
    }
    free(ifc.ifc_buf);
    return 0;
}

static int
refresh_net_dev_ipv6_addr(pmInDom indom)
{
    FILE *fp;
    char addr6p[8][5];
    char addr6[40], devname[20+1];
    char addr[INET6_ADDRSTRLEN];
    char buf[MAXPATHLEN];
    struct sockaddr_in6 sin6;
    int sts, plen, scope, dad_status, if_idx;
    net_addr_t *netip;
    static uint32_t cache_err;

    if ((fp = linux_statsfile("/proc/net/if_inet6", buf, sizeof(buf))) == NULL)
	return 0;

    while (fscanf(fp, "%4s%4s%4s%4s%4s%4s%4s%4s %02x %02x %02x %02x %20s\n",
		  addr6p[0], addr6p[1], addr6p[2], addr6p[3],
		  addr6p[4], addr6p[5], addr6p[6], addr6p[7],
		  &if_idx, &plen, &scope, &dad_status, devname) != EOF) {
	sts = pmdaCacheLookupName(indom, devname, NULL, (void **)&netip);
	if (sts == PM_ERR_INST || (sts >= 0 && netip == NULL)) {
	    /* first time since re-loaded, else new one */
	    netip = (net_addr_t *)calloc(1, sizeof(net_addr_t));
	}
	else if (sts < 0) {
	    if (cache_err++ < 10) {
		fprintf(stderr, "refresh_net_dev_ipv6_addr: "
				"pmdaCacheLookupName(%s, %s, ...) failed: %s\n",
		    pmInDomStr(indom), devname, pmErrStr(sts));
	    }
	    continue;
	}
	if ((sts = pmdaCacheStore(indom, PMDA_CACHE_ADD, devname, (void *)netip)) < 0) {
	    if (cache_err++ < 10) {
		fprintf(stderr, "refresh_net_dev_ipv6_addr: "
			"pmdaCacheStore(%s, PMDA_CACHE_ADD, %s, "
			PRINTF_P_PFX "%p) failed: %s\n",
		    pmInDomStr(indom), devname, netip, pmErrStr(sts));
	    }
	    continue;
	}

	sprintf(addr6, "%s:%s:%s:%s:%s:%s:%s:%s",
		addr6p[0], addr6p[1], addr6p[2], addr6p[3],
		addr6p[4], addr6p[5], addr6p[6], addr6p[7]);
	if (inet_pton(AF_INET6, addr6, sin6.sin6_addr.s6_addr) != 1)
	    continue;

	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = 0;
	if (!inet_ntop(AF_INET6, &sin6.sin6_addr, addr, INET6_ADDRSTRLEN))
	    continue;
	snprintf(netip->ipv6, sizeof(netip->ipv6), "%s/%d", addr, plen);
	netip->ipv6scope = (uint16_t)scope;
	netip->has_ipv6 = 1;

	refresh_net_hw_addr(devname, netip);
    }
    fclose(fp);
    return 0;
}

/*
 * This separate indom provides the addresses for all interfaces including
 * aliases (e.g. eth0, eth0:0, eth0:1, etc) - this is what ifconfig does.
 */
int
refresh_net_dev_addr(pmInDom indom)
{
    int sts = 0;
    net_addr_t*p;

    for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	if ((sts = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if (!pmdaCacheLookup(indom, sts, NULL, (void **)&p) || !p)
	    continue;
	p->has_inet = 0;
	p->has_ipv6 = 0;
	p->has_hw   = 0;
    }

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    sts |= refresh_net_dev_ipv4_addr(indom);
    sts |= refresh_net_dev_ipv6_addr(indom);

    pmdaCacheOp(indom, PMDA_CACHE_SAVE);
    return sts;
}

char *
lookup_ipv6_scope(int scope)
{
    switch (scope) {
    case IPV6_ADDR_ANY:
        return "Global";
    case IPV6_ADDR_LINKLOCAL:
        return "Link";
    case IPV6_ADDR_SITELOCAL:
        return "Site";
    case IPV6_ADDR_COMPATv4:
        return "Compat";
    case IPV6_ADDR_LOOPBACK:
        return "Host";
    }
    return "Unknown";
}
