/*
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

#ifndef _INDOM_H
#define _INDOM_H

enum {
	TG3_INDOM = 0,		/* 0 - tg3 */

	NUM_INDOMS		/* one more than highest numbered cluster */
};

extern pmInDom linux_indom(int);
#define INDOM(i) linux_indom(i)

extern pmdaIndom *linux_pmda_indom(int);
#define PMDAINDOM(i) linux_pmda_indom(i)

/*
 * static string dictionary - one copy of oft-repeated strings;
 * implemented using STRINGS_INDOM and pmdaCache(3) routines.
 */
char *linux_strings_lookup(int);
int linux_strings_insert(const char *);

#endif /* _INDOM_H */
