#
# Copyright (c) 2000,2003,2004,2008 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2007-2010 Aconex.  All Rights Reserved.
# Copyright (c) 2013 Red Hat.
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#

TOPDIR = ../../..
include	$(TOPDIR)/src/include/builddefs

IAM		= ethtool_stats
DOMAIN		= ETHTOOL_STATS
CMDTARGET	= pmdaethtool_stats
LIBTARGET	= pmda_ethtool_stats.so
PMDAINIT	= ethtool_stats_init
PMDADIR		= $(PCP_PMDAS_DIR)/$(IAM)
LOGREWRITEDIR	= $(PCP_VAR_DIR)/config/pmlogrewrite
CONF_LINE	= "linux	60	dso	$(PMDAINIT)	$(PMDADIR)/$(LIBTARGET)"

CFILES		= pmda.c proc_net_dev.c 

HFILES		= clusters.h indom.h \
		  proc_net_dev.h sysfs_kernel.h 

VERSION_SCRIPT	= exports
HELPTARGETS	= help.dir help.pag
LSRCFILES 	= help root_ethtool
LDIRT		= $(HELPTARGETS) domain.h $(VERSION_SCRIPT)

LLDLIBS		= $(PCP_PMDALIB)
LCFLAGS		= $(INVISIBILITY)

# Uncomment these flags for profiling
# LCFLAGS	+= -pg
# LLDFLAGS	+= -pg

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me: domain.h $(LIBTARGET) $(CMDTARGET) $(HELPTARGETS)
	@if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	fi

install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 domain.h help $(HELPTARGETS) $(PMDADIR)
	$(INSTALL) -m 755 $(LIBTARGET) $(CMDTARGET) $(PMDADIR)
	$(INSTALL) -m 644 root_ethtool $(PCP_VAR_DIR)/pmns/root_ethtool
else
build-me:
install:
endif

default_pcp : default

install_pcp : install

$(HELPTARGETS) : help
	$(RUN_IN_BUILD_ENV) $(TOPDIR)/src/newhelp/newhelp -n root_ethtool -v 2 -o help < help

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

pmda.o:	clusters.h
pmda.o:	domain.h
pmda.o proc_net_dev.o:	proc_net_dev.h
pmda.o:	$(VERSION_SCRIPT)
