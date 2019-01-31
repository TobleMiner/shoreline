#include "workqueue.h"

#include <errno.h>

#ifdef SHORELINE_NUMA
#include <numa.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct workqueue* workqueues;
static int num_workqueues = 0;

static int num_workqueue_items = 0;

static void* work_thread(void* priv) {
	int err;
	struct workqueue* wqueue = (struct workqueue*)priv;
	// If there is more than one workqueue we need to take care of allocation policies
#ifdef SHORELINE_NUMA
	if(num_workqueues > 1) {
		numa_set_preferred(wqueue->numa_node);
	}
#endif
	pthread_mutex_lock(&wqueue->lock);
	while(!wqueue->do_exit) {
		pthread_cond_wait(&wqueue->cond, &wqueue->lock);
		while(!(llist_is_empty(&wqueue->entries) || wqueue->do_exit)) {
			struct llist_entry* llentry = llist_get_entry(&wqueue->entries, 0);
			struct workqueue_entry* entry = llist_entry_get_value(llentry, struct workqueue_entry, list);
			llist_remove(llentry);
			num_workqueue_items--;
			if((err = entry->cb(entry->priv))) {
				if(!entry->err) {
					if(entry->cleanup) {
						entry->cleanup(err, entry->priv);
					}
					goto next;
				}
				if(entry->err(err, entry->priv)) {
					if(entry->cleanup) {
						entry->cleanup(err, entry->priv);
					}
					pthread_mutex_unlock(&wqueue->lock);
					free(entry);
					goto fail;
				}
			}
next:
			free(entry);
		}
	}

fail:
	return NULL;
}

static void stop_workqueue(struct workqueue* wqueue) {
	wqueue->do_exit = true;
	pthread_cond_broadcast(&wqueue->cond);
	pthread_join(wqueue->thread, NULL);
	while(!llist_is_empty(&wqueue->entries)) {
		struct llist_entry* llentry = llist_get_entry(&wqueue->entries, 0);
		struct workqueue_entry* entry = llist_entry_get_value(llentry, struct workqueue_entry, list);
		llist_remove(llentry);
		if(entry->cleanup) {
			entry->cleanup(0, entry->priv);
		}
		free(entry);
	}
}


// TODO: Handle CPU hotplug?
int workqueue_init() {
	int err = 0, i;

	num_workqueues = 1;
#ifdef SHORELINE_NUMA
	if(numa_available()) {
		num_workqueues = numa_max_node();
	}
#endif

	workqueues = calloc(num_workqueues, sizeof(struct workqueue));
	if(!workqueues) {
		err = -ENOMEM;
		goto fail;
	}

	for(i = 0; i < num_workqueues; i++) {
		struct workqueue* wqueue = &workqueues[i];
		wqueue->numa_node = i;
		pthread_mutex_init(&wqueue->lock, NULL);
		pthread_cond_init(&wqueue->cond, NULL);
		llist_init(&wqueue->entries);

		if((err = -pthread_create(&wqueue->thread, NULL, work_thread, wqueue))) {
			goto fail_threads;
		}
	}

	return 0;

fail_threads:
	while(i-- > 0) {
		stop_workqueue(&workqueues[i]);
	}
	free(workqueues);
fail:
	return err;
}

void workqueue_deinit() {
	int i;
	for(i = 0; i < num_workqueues; i++) {
		stop_workqueue(&workqueues[i]);
	}
	num_workqueues = 0;
	free(workqueues);
}

int workqueue_enqueue(unsigned numa_node, void* priv, wqueue_cb cb, wqueue_err err, wqueue_cleanup cleanup) {
	struct workqueue* wqueue;
	struct workqueue_entry* entry;

	if(numa_node >= num_workqueues) {
		return -EINVAL;
	}

	entry = calloc(1, sizeof(struct workqueue_entry));
	if(!entry) {
		return -ENOMEM;
	}

	entry->priv = priv;
	entry->cb = cb;
	entry->err = err;
	entry->cleanup = cleanup;

	wqueue = &workqueues[numa_node];
	pthread_mutex_lock(&wqueue->lock);
	llist_append(&wqueue->entries, &entry->list);
	num_workqueue_items++;
	pthread_mutex_unlock(&wqueue->lock);
	pthread_cond_broadcast(&wqueue->cond);

	return 0;
}
