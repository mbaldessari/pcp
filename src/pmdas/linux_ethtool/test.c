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

typedef unsigned long long u64;
typedef __uint32_t u32;
typedef __uint16_t u16;
typedef __uint8_t u8;
typedef __int32_t s32;

#define ETHTOOL_GDRVINFO        0x00000003 /* Get driver info. */
#define ETHTOOL_GSTRINGS        0x0000001b /* get specified string set */
#define ETHTOOL_GSTATS          0x0000001d /* get NIC-specific statistics */
#define ETHTOOL_GSSET_INFO      0x00000037 /* Get string set info */

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

int send_ioctl(struct cmd_context *ctx, void *cmd)
{
        ctx->ifr.ifr_data = cmd;
        return ioctl(ctx->fd, SIOCETHTOOL, &ctx->ifr);
}

static struct ethtool_gstrings *
get_stringset(struct cmd_context *ctx, enum ethtool_stringset set_id,
              ptrdiff_t drvinfo_offset, int null_terminate)
{
        struct {
                struct ethtool_sset_info hdr;
                u32 buf[1];
        } sset_info;
        struct ethtool_drvinfo drvinfo;
        u32 len, i;
        struct ethtool_gstrings *strings;

        sset_info.hdr.cmd = ETHTOOL_GSSET_INFO;
        sset_info.hdr.reserved = 0;
        sset_info.hdr.sset_mask = 1ULL << set_id;
        if (send_ioctl(ctx, &sset_info) == 0) {
                len = sset_info.hdr.sset_mask ? sset_info.hdr.data[0] : 0;
        } else if (errno == EOPNOTSUPP && drvinfo_offset != 0) {
                /* Fallback for old kernel versions */
                drvinfo.cmd = ETHTOOL_GDRVINFO;
                if (send_ioctl(ctx, &drvinfo))
                        return NULL;
                len = *(u32 *)((char *)&drvinfo + drvinfo_offset);
        } else {
                return NULL;
        }

        strings = calloc(1, sizeof(*strings) + len * ETH_GSTRING_LEN);
        if (!strings)
                return NULL;

        strings->cmd = ETHTOOL_GSTRINGS;
        strings->string_set = set_id;
        strings->len = len;
        if (len != 0 && send_ioctl(ctx, strings)) {
                free(strings);
                return NULL;
        }

        if (null_terminate)
                for (i = 0; i < len; i++)
                        strings->data[(i + 1) * ETH_GSTRING_LEN - 1] = 0;

        return strings;
}

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

	*argp++;
	printf("dev: %s\n", *argp);
	ctx.devname = *argp;
	memset(&ctx.ifr, 0, sizeof(ctx.ifr));
	strcpy(ctx.ifr.ifr_name, ctx.devname);

	ctx.fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (ctx.fd < 0) {
		perror("Cannot get control socket");
		return -1;
	}

	do_gstats(&ctx);
}
