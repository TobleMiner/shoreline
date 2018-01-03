#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../llist.h"

#include "main.h"

void print_list(struct llist* list) {
	printf("===LIST BEGIN===\n");
	struct llist_entry* cursor;
	llist_for_each(list, cursor) {
		struct data* data = llist_entry_get_value(cursor, struct data, list);
		printf("%d\n", data->value);
	}
	printf("====LIST END====\n");
}

int main(int argc, char** argv) {
	struct llist llist;
	llist_init(&llist);

	struct data one = { .value = 1, .list = LLIST_ENTRY_INIT };
	struct data two = { .value = 2, .list = LLIST_ENTRY_INIT };
	struct data three = { .value = 3, .list = LLIST_ENTRY_INIT };

	llist_append(&llist, &one.list);
	llist_append(&llist, &two.list);
	llist_append(&llist, &three.list);

	print_list(&llist);

	llist_remove(&two.list);

	print_list(&llist);

	llist_append(&llist, &two.list);

	print_list(&llist);

	llist_remove(&two.list);

	print_list(&llist);

	llist_remove(&one.list);

	print_list(&llist);

	llist_remove(&three.list);

	print_list(&llist);
}
