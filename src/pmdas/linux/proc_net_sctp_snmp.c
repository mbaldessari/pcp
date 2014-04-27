/*
 * Copyright (c) 2014 Red Hat.
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
#include "proc_net_sctp_snmp.h"
#include <ctype.h>

extern proc_net_sctp_snmp_t	_pm_proc_net_sctp;

typedef struct {
    const char		*field;
    __uint64_t		*offset;
} sctp_snmp_fields_t;

sctp_snmp_fields_t sctp_snmp_fields[] = {
    { .field = "SctpCurrEstab",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPCURRESTAB] },
    { .field = "SctpActiveEstabs",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPACTIVEESTABS] },
    { .field = "SctpPassiveEstabs",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPPASSIVEESTABS] },
    { .field = "SctpAborteds",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPABORTEDS] },
    { .field = "SctpShutdowns",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPSHUTDOWNS] },
    { .field = "SctpOutOfBlues",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPOUTOFBLUES] },
    { .field = "SctpChecksumErrors",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPCHECKSUMERRORS] },
    { .field = "SctpOutCtrlChunks",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPOUTCTRLCHUNKS] },
    { .field = "SctpOutOrderChunks",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPOUTORDERCHUNKS] },
    { .field = "SctpOutUnorderChunks",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPOUTUNORDERCHUNKS] },
    { .field = "SctpInCtrlChunks",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPINCTRLCHUNKS] },
    { .field = "SctpInOrderChunks",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPINORDERCHUNKS] },
    { .field = "SctpInUnorderChunks",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPINUNORDERCHUNKS] },
    { .field = "SctpFragUsrMsgs",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPFRAGUSRMSGS] },
    { .field = "SctpReasmUsrMsgs",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPREASMUSRMSGS] },
    { .field = "SctpOutSCTPPacks",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPOUTSCTPPACKS] },
    { .field = "SctpInSCTPPacks",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPINSCTPPACKS] },
    { .field = "SctpT1InitExpireds",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPT1INITEXPIREDS] },
    { .field = "SctpT1CookieExpireds",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPT1COOKIEEXPIREDS] },
    { .field = "SctpT2ShutdownExpireds",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPT2SHUTDOWNEXPIREDS] },
    { .field = "SctpT3RtxExpireds",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPT3RTXEXPIREDS] },
    { .field = "SctpT4RtoExpireds",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPT4RTOEXPIREDS] },
    { .field = "SctpT5ShutdownGuardExpireds",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPT5SHUTDOWNGUARDEXPIREDS] },
    { .field = "SctpDelaySackExpireds",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPDELAYSACKEXPIREDS] },
    { .field = "SctpAutocloseExpireds",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPAUTOCLOSEEXPIREDS] },
    { .field = "SctpT3Retransmits",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPT3RETRANSMITS] },
    { .field = "SctpPmtudRetransmits",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPPMTUDRETRANSMITS] },
    { .field = "SctpFastRetransmits",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPFASTRETRANSMITS] },
    { .field = "SctpInPktSoftirq",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPINPKTSOFTIRQ] },
    { .field = "SctpInPktBacklog",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPINPKTBACKLOG] },
    { .field = "SctpInPktDiscards",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPINPKTDISCARDS] },
    { .field = "SctpInDataChunkDiscards",
     .offset = &_pm_proc_net_sctp.snmp[_PM_SCTP_SNMP_SCTPINDATACHUNKDISCARDS] },
};


static void
get_fields(sctp_snmp_fields_t *fields, int count, char *line)
{
    char *p, *label;
    unsigned long value;
    int i;

    /* sctp/snmp lines are in the form: %-32s\t%ld\n */
    p = line;
    while (!isblank(*p) && *p != '\n')
        p++;
    *p = '\0';
    label = line;
    while (*p != '\t' && *p != '\n')
        p++;

    value = strtoull(p, NULL, 10);
    __pmNotifyErr(LOG_ERR, "%d:%s - %s - %lu\n", count, fields[count].field, label, value);

    /* optimize for the case where values exported by the kernel
     * are complete and in the order as codified in this pmda
     */
    if (strcmp(fields[count].field, label) == 0)
        *fields[count].offset = value;
    else {
        for (i = 0; fields[i].field; i++) {
            if (strcmp(fields[i].field, label) != 0)
               continue;
            *fields[i].offset = value;
            break;
        }
    }
}

#define SCTP_SNMP_OFFSET(ii, pp) (int64_t *)((char *)pp + \
    (__psint_t)sctp_snmp_fields[ii].offset - (__psint_t)&_pm_proc_net_sctp.snmp)

static void
init_refresh_proc_net_sctp_snmp(proc_net_sctp_snmp_t *sctp_snmp)
{
    int		i;

    /* initially, all marked as "no value available" */
    for (i = 0; sctp_snmp_fields[i].field != NULL; i++)
	*(SCTP_SNMP_OFFSET(i, sctp_snmp)) = -1;
}

int
refresh_proc_net_sctp_snmp(proc_net_sctp_snmp_t *sctp_snmp)
{
    /* Need a sufficiently large value to hold a full line */
    char	line[512];
    int		count = 0;
    FILE	*fp;

    init_refresh_proc_net_sctp_snmp(sctp_snmp);
    if ((fp = fopen("/proc/net/sctp/snmp", "r")) == NULL)
	return -oserror();
    while (fgets(line, sizeof(line), fp) != NULL) {
	get_fields(sctp_snmp_fields, count, line);
	count++;
    }
    fclose(fp);
    return 0;
}
