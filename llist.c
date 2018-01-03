#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

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


void llist_append(struct llist* llist, struct llist_entry* entry) {
	pthread_mutex_lock(&llist->lock);
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
	pthread_mutex_unlock(&llist->lock);
}

void llist_remove(struct llist_entry* entry) {
	struct llist* llist = entry->list;
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
}
