#ifndef _FRONTEND_H_
#define _FRONTEND_H_

#include <stdbool.h>

#include "framebuffer.h"
#include "llist.h"

struct frontend;

#define FRONTEND_ALLOC(name) int (*name)(struct frontend** res, struct fb* fb, void* priv)
#define FRONTEND_START(name) int (*name)(struct frontend* front)
#define FRONTEND_FREE(name) void (*name)(struct frontend* front)
#define FRONTEND_UPDATE(name) int (*name)(struct frontend* front)
#define FRONTEND_DRAW_STRING(name) int (*name)(struct frontend* front, unsigned x, unsigned y, char* str)

typedef int (*frontend_alloc)(struct frontend** res, struct fb* fb, void* priv);
typedef int (*frontend_start)(struct frontend* front);
typedef void (*frontend_free)(struct frontend* front);
typedef int (*frontend_update)(struct frontend* front);
typedef int (*frontend_draw_string)(struct frontend* front, unsigned x, unsigned y, char* str);

struct frontend_ops {
	FRONTEND_ALLOC(alloc);
	FRONTEND_START(start);
	FRONTEND_FREE(free);
	FRONTEND_UPDATE(update);
	FRONTEND_DRAW_STRING(draw_string);
};

struct frontend_arg {
	char* name;
	int (*configure)(struct frontend* front, char* value);
};

struct frontend_def {
	char* name;
	const struct frontend_ops* ops;
	bool handles_signals;
	const struct frontend_arg* args;
};

#define DECLARE_FRONTEND(name, frontname, frontops, sig) \
	struct frontend_def name = {frontname, frontops, sig, NULL}

#define DECLARE_FRONTEND_SIG(name, frontname, frontops) \
	DECLARE_FRONTEND(name, frontname, frontops, true)

#define DECLARE_FRONTEND_NOSIG(name, frontname, frontops) \
	DECLARE_FRONTEND(name, frontname, frontops, false)

#define DECLARE_FRONTEND_ARGS(name, frontname, frontops, sig, arg_names) \
	struct frontend_def name = {frontname, frontops, sig, arg_names}

#define DECLARE_FRONTEND_SIG_ARGS(name, frontname, frontops, arg_names) \
	DECLARE_FRONTEND_ARGS(name, frontname, frontops, true, arg_names)

#define DECLARE_FRONTEND_NOSIG_ARGS(name, frontname, frontops, arg_names) \
	DECLARE_FRONTEND_ARGS(name, frontname, frontops, false, arg_names)

#define frontend_alloc(def, front, fb, priv) ((def)->ops->alloc((front), (fb), (priv)))
#define frontend_start(front) ((front)->def->ops->start((front)))
#define frontend_free(front) ((front)->def->ops->free((front)))
#define frontend_update(front) ((front)->def->ops->update((front)))
#define frontend_draw_string(front, x, y, str) ((front)->def->ops->draw_string((front), (x), (y), (str)))

#define frontend_can_configure(front) (!!(front)->def->args)
#define frontend_can_start(front) (!!(front)->def->ops->start)
#define frontend_can_draw_string(front) (!!(front)->def->ops->draw_string)

struct frontend {
	struct frontend_def* def;
	struct llist_entry list;
};

struct frontend_id {
	char* id;
	struct frontend_def* def;
};

struct frontend_def* frontend_get_def(char* id);
char* frontend_spec_extract_name(char* spec);
int frontend_configure(struct frontend* front, char* options);

#endif
