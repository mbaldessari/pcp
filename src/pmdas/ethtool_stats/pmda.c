/*
 * Linux PMDA
 *
 * Copyright (c) 2012-2013 Red Hat.
 * Copyright (c) 2007-2011 Aconex.  All Rights Reserved.
 * Copyright (c) 2002 International Business Machines Corp.
 * Copyright (c) 2000,2004,2007-2008 Silicon Graphics, Inc.  All Rights Reserved.
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
#undef LINUX /* defined in NSS/NSPR headers as something different, which we do not need. */
#include "domain.h"

#include <ctype.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <utmp.h>
#include <pwd.h>
#include <grp.h>

#include "clusters.h"
#include "indom.h"

#include "proc_net_dev.h"

static int		_isDSO = 1;	/* =0 I am a daemon */
static char		*username;

/* globals */
/*
 * Metric Instance Domains (statically initialized ones only)
 */

static pmdaIndom indomtab[] = {
    { TG3_INDOM, 0, NULL },
};

/*
 * all metrics supported in this PMDA - one table entry for each
 */

static pmdaMetric metrictab[] = {
/*
 * tg3 cluster
 */

/* network.ethtool.tg3.rx_octets */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,0), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_fragments */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,1), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_ucast_packets */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,2), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_mcast_packets */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,3), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_bcast_packets */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,4), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_fcs_errors */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,5), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_align_errors */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,6), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_xon_pause_rcvd */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,7), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_xoff_pause_rcvd */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,8), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_mac_ctrl_rcvd */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,9), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_xoff_entered */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,10), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_frame_too_long_errors */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,11), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_jabbers */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,12), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_undersize_packets */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,13), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_in_length_errors */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,14), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_out_length_errors */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,15), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_64_or_less_octet_packets */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,16), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_65_to_127_octet_packets */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,17), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_128_to_255_octet_packets */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,18), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_256_to_511_octet_packets */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,19), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_512_to_1023_octet_packets */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,20), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_1024_to_1522_octet_packets */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,21), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_1523_to_2047_octet_packets */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,22), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_2048_to_4095_octet_packets */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,23), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_4096_to_8191_octet_packets */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,24), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_8192_to_9022_octet_packets */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,25), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_octets */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,26), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_collisions */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,27), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_xon_sent */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,28), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_xoff_sent */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,29), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_flow_control */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,30), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_mac_errors */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,31), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_single_collisions */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,32), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_mult_collisions */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,33), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_deferred */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,34), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_excessive_collisions */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,35), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_late_collisions */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,36), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_collide_2times */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,37), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_collide_3times */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,38), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_collide_4times */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,39), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_collide_5times */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,40), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_collide_6times */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,41), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_collide_7times */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,42), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_collide_8times */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,43), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_collide_9times */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,44), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_collide_10times */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,45), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_collide_11times */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,46), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_collide_12times */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,47), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_collide_13times */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,48), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_collide_14times */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,49), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_collide_15times */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,50), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_ucast_packets */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,51), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_mcast_packets */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,52), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_bcast_packets */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,53), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_carrier_sense_errors */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,54), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_discards */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,55), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_errors */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,56), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.dma_writeq_full */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,57), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.dma_write_prioq_full */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,58), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rxbds_empty */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,59), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_discards */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,60), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_errors */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,61), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.rx_threshold_hit */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,62), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.dma_readq_full */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,63), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.dma_read_prioq_full */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,64), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.tx_comp_queue_full */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,65), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.ring_set_send_prod_index */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,66), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.ring_status_update */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,67), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.nic_irqs */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,68), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.nic_avoided_irqs */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,69), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.nic_tx_threshold_hit */
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,70), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.ethtool.tg3.mbuf_lwm_thresh_hit*/
    { NULL, 
      { PMDA_PMID(CLUSTER_TG3,71), PM_TYPE_U64, TG3_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
};

static void
linux_refresh(pmdaExt *pmda, int *need_refresh)
{
    if (need_refresh[CLUSTER_TG3])
	refresh_proc_net_dev(INDOM(TG3_INDOM));
}

static int
linux_instance(pmInDom indom, int inst, char *name, __pmInResult **result, pmdaExt *pmda)
{
    __pmInDom_int	*indomp = (__pmInDom_int *)&indom;
    int			need_refresh[NUM_CLUSTERS];

    memset(need_refresh, 0, sizeof(need_refresh));
    switch (indomp->serial) {
    case TG3_INDOM:
    	need_refresh[CLUSTER_TG3]++;
	break;
    /* no default label : pmdaInstance will pick up errors */
    }

    linux_refresh(pmda, need_refresh);
    return pmdaInstance(indom, inst, name, result, pmda);
}

/*
 * callback provided to pmdaFetch
 */

static int
linux_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    int			i;
    int			sts;
    net_interface_t	*netip;

    if (mdesc->m_user == NULL) 
    switch (idp->cluster) {
    case CLUSTER_TG3: /* network.ethtool.tg3 */
	sts = pmdaCacheLookup(INDOM(CLUSTER_INDOM), inst, NULL, (void **)&netip);
	if (sts < 0)
	    return sts;
	if (idp->item <= 15) {
	    /* network.interface.{in,out} */
	    atom->ull = netip->counters[idp->item];
	}
	else
	switch (idp->item) {
	case 16: /* network.interface.total.bytes */
	    atom->ull = netip->counters[0] + netip->counters[8];
	    break;
	case 17: /* network.interface.total.packets */
	    atom->ull = netip->counters[1] + netip->counters[9];
	    break;
	case 18: /* network.interface.total.errors */
	    atom->ull = netip->counters[2] + netip->counters[10];
	    break;
	case 19: /* network.interface.total.drops */
	    atom->ull = netip->counters[3] + netip->counters[11];
	    break;
	case 20: /* network.interface.total.mcasts */
	    /*
	     * NOTE: there is no network.interface.out.mcasts metric
	     * so this total only includes network.interface.in.mcasts
	     */
	    atom->ull = netip->counters[7];
	    break;
	case 21: /* network.interface.mtu */
	    if (!netip->ioc.mtu)
		return 0;
	    atom->ul = netip->ioc.mtu;
	    break;
	case 22: /* network.interface.speed */
	    if (!netip->ioc.speed)
		return 0;
	    atom->f = ((float)netip->ioc.speed * 1000000) / 8 / 1024 / 1024;
	    break;
	case 23: /* network.interface.baudrate */
	    if (!netip->ioc.speed)
		return 0;
	    atom->ul = ((long long)netip->ioc.speed * 1000000 / 8);
	    break;
	case 24: /* network.interface.duplex */
	    if (!netip->ioc.duplex)
		return 0;
	    atom->ul = netip->ioc.duplex;
	    break;
	case 25: /* network.interface.up */
	    atom->ul = netip->ioc.linkup;
	    break;
	case 26: /* network.interface.running */
	    atom->ul = netip->ioc.running;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    default: /* unknown cluster */
	return PM_ERR_PMID;
    }

    return 1;
}


static int
linux_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int		i;
    int		need_refresh[NUM_CLUSTERS];

    memset(need_refresh, 0, sizeof(need_refresh));
    for (i=0; i < numpmid; i++) {
	__pmID_int *idp = (__pmID_int *)&(pmidlist[i]);
    }

    linux_refresh(pmda, need_refresh);
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
linux_text(int ident, int type, char **buf, pmdaExt *pmda)
{
    if ((type & PM_TEXT_PMID) == PM_TEXT_PMID) {
	int sts = pmdaDynamicLookupText(ident, type, buf, pmda);
	if (sts != -ENOENT)
	    return sts;
    }
    return pmdaText(ident, type, buf, pmda);
}

static int
linux_pmid(const char *name, pmID *pmid, pmdaExt *pmda)
{
    pmdaNameSpace *tree = pmdaDynamicLookupName(pmda, name);
    return pmdaTreePMID(tree, name, pmid);
}

static int
linux_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    pmdaNameSpace *tree = pmdaDynamicLookupPMID(pmda, pmid);
    return pmdaTreeName(tree, pmid, nameset);
}

static int
linux_children(const char *name, int flag, char ***kids, int **sts, pmdaExt *pmda)
{
    pmdaNameSpace *tree = pmdaDynamicLookupName(pmda, name);
    return pmdaTreeChildren(tree, name, flag, kids, sts);
}

pmInDom
linux_indom(int serial)
{
    return indomtab[serial].it_indom;
}

pmdaIndom *
linux_pmda_indom(int serial)
{
    return &indomtab[serial];
}

/*
 * Helper routines for accessing a generic static string dictionary
 */

char *
linux_strings_lookup(int index)
{
    char *value;
    pmInDom dict = INDOM(STRINGS_INDOM);

    if (pmdaCacheLookup(dict, index, &value, NULL) == PMDA_CACHE_ACTIVE)
	return value;
    return NULL;
}

int
linux_strings_insert(const char *buf)
{
    pmInDom dict = INDOM(STRINGS_INDOM);
    return pmdaCacheStore(dict, PMDA_CACHE_ADD, buf, NULL);
}

/*
 * Initialise the agent (both daemon and DSO).
 */

void
__PMDA_INIT_CALL
ethtool_stats_init(pmdaInterface *dp)
{
    int		i, major, minor, point;
    size_t	nmetrics, nindoms;
    __pmID_int	*idp;

    if (_isDSO) {
	char helppath[MAXPATHLEN];
	int sep = __pmPathSeparator();
	snprintf(helppath, sizeof(helppath), "%s%c" "linux" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_4, "ethtool_stats", helppath);
    } else {
	__pmSetProcessIdentity(username);
    }

    if (dp->status != 0)
	return;

    dp->version.four.instance = linux_instance;
    dp->version.four.fetch = linux_fetch;
    dp->version.four.text = linux_text;
    dp->version.four.pmid = linux_pmid;
    dp->version.four.name = linux_name;
    dp->version.four.children = linux_children;
    pmdaSetFetchCallBack(dp, linux_fetchCallBack);

    for (i = 0; i < sizeof(metrictab)/sizeof(pmdaMetric); i++) {
	idp = (__pmID_int *)&(metrictab[i].m_desc.pmid);
	if (metrictab[i].m_desc.type == PM_TYPE_NOSUPPORT)
	    fprintf(stderr, "Bad kernel metric descriptor type (%u.%u)\n",
			    idp->cluster, idp->item);
    }

    nindoms = sizeof(indomtab)/sizeof(indomtab[0]);
    nmetrics = sizeof(metrictab)/sizeof(metrictab[0]);

    pmdaSetFlags(dp, PMDA_EXT_FLAG_HASHED);
    pmdaInit(dp, indomtab, nindoms, metrictab, nmetrics);

    /* string metrics use the pmdaCache API for value indexing */
    pmdaCacheOp(INDOM(STRINGS_INDOM), PMDA_CACHE_STRINGS);
}


static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmProgname);
    fputs("Options:\n"
	  "  -d domain  use domain (numeric) for metrics domain of PMDA\n"
	  "  -l logfile write log into logfile rather than using default log name\n"
	  "  -U username  user account to run under (default \"pcp\")\n",
	  stderr);		
    exit(1);
}

/*
 * Set up the agent if running as a daemon.
 */

int
main(int argc, char **argv)
{
    int			sep = __pmPathSeparator();
    int			err = 0;
    int			c;
    pmdaInterface	dispatch;
    char		helppath[MAXPATHLEN];

    _isDSO = 0;
    __pmSetProgname(argv[0]);
    __pmGetUsername(&username);

    snprintf(helppath, sizeof(helppath), "%s%c" "linux" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_4, pmProgname, ETHTOOL_STATS, "ethtool_stats.log", helppath);

    while ((c = pmdaGetOpt(argc, argv, "D:d:l:U:?", &dispatch, &err)) != EOF) {
	switch(c) {
	case 'U':
	    username = optarg;
	    break;
	default:
	    err++;
	}
    }
    if (err)
	usage();

    pmdaOpenLog(&dispatch);
    ethtool_stats_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
