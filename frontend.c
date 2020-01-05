#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "frontend.h"

#ifdef FEATURE_SDL
extern struct frontend_def front_sdl;
#endif
#ifdef FEATURE_VNC
extern struct frontend_def front_vnc;
#endif
#ifdef FEATURE_STATISTICS
extern struct frontend_def front_statistics;
#endif

struct frontend_id frontends[] = {
#ifdef FEATURE_SDL
	{ "sdl", &front_sdl },
#endif
#ifdef FEATURE_VNC
	{ "vnc", &front_vnc },
#endif
#ifdef FEATURE_STATISTICS
	{ "statistics", &front_statistics },
#endif
	{ NULL, NULL }
};

struct frontend_def* frontend_get_def(char* id) {
	struct frontend_id* front = frontends;
	for(; front->def != NULL; front++) {
		if(strcmp(id, front->id) == 0) {
			return front->def;
		}
	}
	return NULL;
}

char* frontend_spec_extract_name(char* spec) {
	char* sep = strchr(spec, ',');
	char* limit = spec + strlen(spec);
	if(sep) {
		*sep = '\0';
		return sep + 1 < limit ? sep + 1 : NULL;
	}
	return NULL;
}

static int frontend_configure_option(struct frontend* front, char* option) {
	char* sep = strchr(option, '=');
	char* sep_limit = strchr(option, ',');
	char* limit = option + strlen(option);
	char* value = NULL;
	const struct frontend_arg* args = front->def->args;
	if(sep && (sep < sep_limit || !sep_limit)) {
		*sep = '\0';
		value = sep + 1 < limit ? sep + 1 : NULL;
	}
	while(args && strlen(args->name)) {
		if(strcmp(option, args->name) == 0) {
			return args->configure(front, value);
		}
		args++;
	}
	return -ENOENT;
}

int frontend_configure(struct frontend* front, char* options) {
	int err;
	char* sep = NULL;
	char* limit = options + strlen(options);
	while(options < limit && (sep = strchr(options, ','))) {
		*sep = '\0';
		if((err = frontend_configure_option(front, options))) {
			return err;
		}
		options = sep + 1;
	}
	if(options < limit) {
		if((err = frontend_configure_option(front, options))) {
			return err;
		}
	}
	return 0;
}
