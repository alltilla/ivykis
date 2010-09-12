/*
 * ivykis, an event handling library
 * Copyright (C) 2010 Lennert Buytenhek
 * Dedicated to Marija Kulikova.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version
 * 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License version 2.1 for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License version 2.1 along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <iv.h>
#include <iv_signal.h>
#include <iv_wait.h>
#include <pthread.h>
#include "thr.h"

struct wait_event {
	struct list_head	list;
	int			status;
	struct rusage		rusage;
};

static int
iv_wait_interest_compare(struct iv_avl_node *_a, struct iv_avl_node *_b)
{
	struct iv_wait_interest *a;
	struct iv_wait_interest *b;

	a = container_of(_a, struct iv_wait_interest, avl_node);
	b = container_of(_b, struct iv_wait_interest, avl_node);

	if (a->pid < b->pid)
		return -1;
	if (a->pid > b->pid)
		return 1;
	return 0;
}

static pthread_mutex_t iv_wait_interests_lock = PTHREAD_MUTEX_INITIALIZER;
static struct iv_avl_tree iv_wait_interests =
	IV_AVL_TREE_INIT(iv_wait_interest_compare);

static struct iv_wait_interest *__iv_wait_interest_find(int pid)
{
	struct iv_avl_node *an;

	an = iv_wait_interests.root;
	while (an != NULL) {
		struct iv_wait_interest *p;

		p = container_of(an, struct iv_wait_interest, avl_node);
		if (pid == p->pid)
			return p;

		if (pid < p->pid)
			an = an->left;
		else
			an = an->right;
	}

	return NULL;
}

static void iv_wait_got_sigchld(void *_dummy)
{
	while (1) {
		pid_t pid;
		int status;
		struct rusage rusage;
		struct wait_event *we;
		struct iv_wait_interest *p;

		pid = wait4(-1, &status, WNOHANG, &rusage);
		if (pid <= 0) {
			if (pid < 0 && errno != ECHILD)
				perror("wait4");
			break;
		}

		we = malloc(sizeof(*we));
		if (we == NULL) {
			fprintf(stderr, "iv_wait_got_sigchld: OOM\n");
			exit(-1);
		}

		we->status = status;
		we->rusage = rusage;

		pthr_mutex_lock(&iv_wait_interests_lock);
		p = __iv_wait_interest_find(pid);
		if (p != NULL) {
			list_add_tail(&we->list, &p->events);
			iv_event_post(&p->ev);
		}
		pthr_mutex_unlock(&iv_wait_interests_lock);

		if (p == NULL)
			free(we);
	}
}

static void iv_wait_completion(void *_this)
{
	struct iv_wait_interest *this = _this;

	this->term = (void **)&this;

	while (!list_empty(&this->events)) {
		struct wait_event *we;

		we = container_of(this->events.next, struct wait_event, list);
		list_del(&we->list);

		this->handler(this->cookie, we->status, &we->rusage);

		free(we);

		if (this == NULL)
			return;
	}

	this->term = NULL;
}

static __thread struct iv_wait_thr_info {
	int			wait_count;
	struct iv_signal	sigchld_interest;
} tinfo;

void iv_wait_interest_register(struct iv_wait_interest *this)
{
	if (!tinfo.wait_count++) {
		tinfo.sigchld_interest.signum = SIGCHLD;
		tinfo.sigchld_interest.exclusive = 1;
		tinfo.sigchld_interest.handler = iv_wait_got_sigchld;
		iv_signal_register(&tinfo.sigchld_interest);
	}

	this->ev.handler = iv_wait_completion;
	this->ev.cookie = this;
	iv_event_register(&this->ev);

	INIT_LIST_HEAD(&this->events);

	this->term = NULL;

	pthr_mutex_lock(&iv_wait_interests_lock);
	iv_avl_tree_insert(&iv_wait_interests, &this->avl_node);
	pthr_mutex_unlock(&iv_wait_interests_lock);
}

void iv_wait_interest_unregister(struct iv_wait_interest *this)
{
	pthr_mutex_lock(&iv_wait_interests_lock);
	iv_avl_tree_delete(&iv_wait_interests, &this->avl_node);
	pthr_mutex_unlock(&iv_wait_interests_lock);

	iv_event_unregister(&this->ev);

	while (!list_empty(&this->events)) {
		struct wait_event *we;

		we = container_of(this->events.next, struct wait_event, list);
		list_del(&we->list);
		free(we);
	}

	if (this->term != NULL)
		*this->term = NULL;

	if (!--tinfo.wait_count)
		iv_signal_unregister(&tinfo.sigchld_interest);
}