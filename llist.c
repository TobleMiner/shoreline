#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>

#include "llist.h"


void llist_init(struct llist* llist) {
	llist->head = NULL;
	llist->tail = NULL;
	pthread_mutex_init(&llist->lock, NULL);
}

void llist_entry_init(struct llist_entry* entry) {
	entry->next = NULL;
	entry->prev = NULL;
	entry->list = NULL;
}


int llist_alloc(struct llist** ret) {
	struct llist* llist = malloc(sizeof(struct llist));
	if(!llist) {
		return -ENOMEM;
	}

	llist_init(llist);
	*ret = llist;

	return 0;
}

void llist_free(struct llist* llist) {
	free(llist);
}


void llist_append(struct llist* llist, struct llist_entry* entry) {
	llist_lock(llist);
	entry->list = llist;
	if(!llist->head || !llist->tail) {
		assert(!llist->tail);
		entry->next = NULL;
		entry->prev = NULL;
		llist->head = entry;
		llist->tail = entry;
	} else {
		entry->next = NULL;
		llist->tail->next = entry;
		entry->prev = llist->tail;
		llist->tail = entry;
	}
	llist_unlock(llist);
}

void llist_remove(struct llist_entry* entry) {
	struct llist* llist = entry->list;
	llist_lock(llist);
	if(entry == llist->head) {
		llist->head = entry->next;
	}
	if(entry == llist->tail) {
		llist->tail = entry->prev;
	}
	if(entry->next) {
		entry->next->prev = entry->prev;
	}
	if(entry->prev) {
		entry->prev->next = entry->next;
	}
	entry->next = NULL;
	entry->prev = NULL;
	entry->list = NULL;
	llist_unlock(llist);
}
