#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <sys/utsname.h>
#include <limits.h>
#include <ctype.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <linux/sockios.h>
#include <net/if.h>

#ifndef MAX_ADDR_LEN
#define MAX_ADDR_LEN	32
#endif

#ifndef HAVE_BE_TYPES
typedef __uint16_t __be16;
typedef __uint32_t __be32;
typedef unsigned long long __be64;
#endif

typedef unsigned long long u64;
typedef __uint32_t u32;
typedef __uint16_t u16;
typedef __uint8_t u8;
typedef __int32_t s32;

#define ETHTOOL_GSTATS          0x0000001d /* get NIC-specific statistics */

#define ETH_GSTRING_LEN         32                                                
enum ethtool_stringset {                                                          
        ETH_SS_TEST             = 0,                                              
        ETH_SS_STATS,                                                             
        ETH_SS_PRIV_FLAGS,                                                        
        ETH_SS_NTUPLE_FILTERS,  /* Do not use, GRXNTUPLE is now deprecated */     
        ETH_SS_FEATURES,                                                          
}; 

/* for passing string sets for data tagging */
struct ethtool_gstrings {                                                         
        u32   cmd;            /* ETHTOOL_GSTRINGS */
        u32   string_set;     /* string set id e.c. ETH_SS_TEST, etc*/
        u32   len;            /* number of strings in the string set */
        u8    data[0];         
};                                                                                

struct ethtool_sset_info {
        u32   cmd;            /* ETHTOOL_GSSET_INFO */
        u32   reserved;
        u64   sset_mask;      /* input: each bit selects an sset to query */
                              /* output: each bit a returned sset */
        u32   data[0];        /* ETH_SS_xxx count, in order, based on bits
                                 in sset_mask.  One bit implies one
                                 __u32, two bits implies two
                                 __u32's, etc. */
};

#define ETHTOOL_FWVERS_LEN      32
#define ETHTOOL_BUSINFO_LEN     32
struct ethtool_drvinfo {
        u32   cmd;
        char    driver[32];     /* driver short name, "tulip", "eepro100" */
        char    version[32];    /* driver version string */
        char    fw_version[ETHTOOL_FWVERS_LEN]; /* firmware version string */
        char    bus_info[ETHTOOL_BUSINFO_LEN];  /* Bus info for this IF. */
                                /* For PCI devices, use pci_name(pci_dev). */
        char    reserved1[32];
        char    reserved2[12];
                                /*
                                 * Some struct members below are filled in
                                 * using ops->get_sset_count().  Obtaining
                                 * this info from ethtool_drvinfo is now
                                 * deprecated; Use ETHTOOL_GSSET_INFO
                                 * instead.
                                 */
        u32   n_priv_flags;   /* number of flags valid in ETHTOOL_GPFLAGS */
        u32   n_stats;        /* number of u64's from ETHTOOL_GSTATS */
        u32   testinfo_len;
        u32   eedump_len;     /* Size of data from ETHTOOL_GEEPROM (bytes) */
        u32   regdump_len;    /* Size of data from ETHTOOL_GREGS (bytes) */
};                                                                                

/* for dumping NIC-specific statistics */
struct ethtool_stats {
        u32   cmd;            /* ETHTOOL_GSTATS */                 
        u32   n_stats;        /* number of u64's being returned */
        u64   data[0];
};      

/* Context for sub-commands */
struct cmd_context {
	const char *devname;    /* net device name */
	int fd;                 /* socket suitable for ethtool ioctl */
	struct ifreq ifr;       /* ifreq suitable for ethtool ioctl */
	int argc;               /* number of arguments to the sub-command */
	char **argp;            /* arguments to the sub-command */
};                                                                           

struct flag_info {
	const char *name;
	u32 value;
};

static int do_gstats(struct cmd_context *ctx)
{
	struct ethtool_gstrings *strings;
	struct ethtool_stats *stats;
	unsigned int n_stats, sz_stats, i;
	int err;

	strings = get_stringset(ctx, ETH_SS_STATS,
				offsetof(struct ethtool_drvinfo, n_stats),
				0);
	if (!strings) {
		perror("Cannot get stats strings information");
		return 96;
	}

	n_stats = strings->len;
	if (n_stats < 1) {
		fprintf(stderr, "no stats available\n");
		free(strings);
		return 94;
	}

	sz_stats = n_stats * sizeof(u64);

	stats = calloc(1, sz_stats + sizeof(struct ethtool_stats));
	if (!stats) {
		fprintf(stderr, "no memory available\n");
		free(strings);
		return 95;
	}

	stats->cmd = ETHTOOL_GSTATS;
	stats->n_stats = n_stats;
	err = send_ioctl(ctx, stats);
	if (err < 0) {
		perror("Cannot get stats information");
		free(strings);
		free(stats);
		return 97;
	}

	/* todo - pretty-print the strings per-driver */
	fprintf(stdout, "NIC statistics:\n");
	for (i = 0; i < n_stats; i++) {
		fprintf(stdout, "     %.*s: %llu\n",
			ETH_GSTRING_LEN,
			&strings->data[i * ETH_GSTRING_LEN],
			stats->data[i]);
	}
	free(strings);
	free(stats);

	return 0;
}

int main(int argc, char **argp)
{
	int (*func)(struct cmd_context *);
	int want_device;
	struct cmd_context ctx;
	int k;

	do_gstats(&ctx);
}
