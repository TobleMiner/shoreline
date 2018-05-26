#ifndef _FRONTEND_H_
#define _FRONTEND_H_

#include <stdbool.h>

#include "framebuffer.h"
#include "llist.h"

struct frontend;

#define FRONTEND_ALLOC(name) int (*name)(struct frontend** res, struct fb* fb, void* priv)
#define FRONTEND_FREE(name) void (*name)(struct frontend* front)
#define FRONTEND_UPDATE(name) int (*name)(struct frontend* front)

typedef int (*frontend_alloc)(struct frontend** res, struct fb* fb, void* priv);
typedef void (*frontend_free)(struct frontend* front);
typedef int (*frontend_update)(struct frontend* front);

struct frontend_ops {
	FRONTEND_ALLOC(alloc);
	FRONTEND_FREE(free);
	FRONTEND_UPDATE(update);
};

struct frontend_def {
	char* name;
	const struct frontend_ops* ops;
	bool handles_signals;
};

#define DECLARE_FRONTEND(name, frontname, frontops, sig) \
	struct frontend_def name = {frontname, frontops, sig}

#define DECLARE_FRONTEND_SIG(name, frontname, frontops) \
	DECLARE_FRONTEND(name, frontname, frontops, true)

#define DECLARE_FRONTEND_NOSIG(name, frontname, frontops) \
	DECLARE_FRONTEND(name, frontname, frontops, false)

struct frontend {
	struct frontend_def* def;
	struct llist_entry list;
};

struct frontend_id {
	char* id;
	struct frontend_def* def;
};

#endif
