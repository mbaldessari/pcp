/*
 * Linux /proc/net_dev metrics cluster
 *
 * Copyright (c) 2013 Red Hat.
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
#include <sys/types.h>
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
    char path[256];
    char line[64];
    char *duplex;

    snprintf(path, sizeof(path), "/sys/class/net/%s/speed", name);
    path[sizeof(path)-1] = '\0';
    netip->ioc.speed = atoi(read_oneline(path, line));

    snprintf(path, sizeof(path), "/sys/class/net/%s/duplex", name);
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
    char path[256];
    char line[64];
    char *value;

    snprintf(path, sizeof(path), "/sys/class/net/%s/address", name);
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

    if ((fp = fopen("/proc/net/dev", "r")) == (FILE *)0)
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
