#ifndef _LLIST_H_
#define _LLIST_H_

#include <pthread.h>

#include "util.h"

struct llist_entry;

struct llist {
	struct llist_entry* head;
	struct llist_entry* tail;
	pthread_mutex_t lock;
};

struct llist_entry {
	struct llist_entry* next;
	struct llist_entry* prev;
	struct llist* list;
};

#define LLIST_ENTRY_INIT { NULL, NULL, NULL }

#define llist_entry_get_value(entry, type, member) \
	container_of(entry, type, member)

#define llist_for_each(list, cursor) \
	for(cursor = (list)->head; (cursor) != NULL; (cursor) = (cursor)->next)

void llist_init(struct llist* llist);
void llist_append(struct llist* llist, struct llist_entry* entry);
void llist_remove(struct llist_entry* entry);

#endif
