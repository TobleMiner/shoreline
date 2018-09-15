#ifndef _WORKQUEUE_H_
#define _WORKQUEUE_H_

#include <pthread.h>
#include <stdbool.h>

#include "llist.h"

typedef int (*wqueue_cb)(void* priv);
typedef int (*wqueue_err)(int err, void* priv);
typedef void (*wqueue_cleanup)(int err, void* priv);

struct workqueue_entry {
	int (*cb)(void* priv);
	int (*err)(int err, void* priv);
	void (*cleanup)(int err, void* priv);
	void* priv;

	struct llist_entry list;
};

struct workqueue {
	struct llist entries;
	unsigned numa_node;
	bool do_exit;

	pthread_t thread;
	pthread_cond_t cond;
	pthread_mutex_t lock;
};

int workqueue_init();
void workqueue_deinit();
int workqueue_enqueue(unsigned numa_node, void* priv, wqueue_cb cb, wqueue_err err, wqueue_cleanup cleanup);

#endif
