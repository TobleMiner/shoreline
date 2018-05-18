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

#define LLIST_ENTRY_INIT ((struct llist_entry){ NULL, NULL, NULL })

#define llist_entry_get_value(entry, type, member) \
	container_of(entry, type, member)

#define llist_for_each(list, cursor) \
	for(cursor = (list)->head; (cursor) != NULL; (cursor) = (cursor)->next)

#define llist_lock(llist) pthread_mutex_lock(&(llist)->lock);
#define llist_unlock(llist) pthread_mutex_unlock(&(llist)->lock);

void llist_init(struct llist* llist);
void llist_entry_init(struct llist_entry* entry);

int llist_alloc(struct llist** ret);
void llist_free(struct llist* ret);


void llist_append(struct llist* llist, struct llist_entry* entry);
void llist_remove(struct llist_entry* entry);

size_t llist_length(struct llist* list);
struct llist_entry* llist_get_entry(struct llist* list, unsigned int index);

#endif
