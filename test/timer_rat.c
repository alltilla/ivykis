/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003 Lennert Buytenhek
 * Dedicated to Marija Kulikova.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <iv.h>

#define NUM		1048570
static struct iv_timer	tim[NUM];

static void handler(void *_t)
{
	struct iv_timer *t = (struct iv_timer *)_t;

	printf("%i\n", (int)(t - tim));

#if 0
	iv_validate_now();
	t->expires = now;
	t->expires.tv_sec += 1;
	iv_register_timer(t);
#endif
}

int main()
{
	int i;

	iv_init();

	iv_validate_now();

	for (i = 0; i < NUM; i++) {
		INIT_IV_TIMER(tim + i);
		tim[i].expires = now;
		tim[i].expires.tv_sec += 1;
		tim[i].expires.tv_nsec += i;
		if (tim[i].expires.tv_nsec >= 1000000000) {
			tim[i].expires.tv_sec++;
			tim[i].expires.tv_nsec -= 1000000000;
		}
		tim[i].cookie = (void *)&tim[i];
		tim[i].handler = handler;
		iv_register_timer(&tim[i]);
	}

	iv_main();

	return 0;
}
