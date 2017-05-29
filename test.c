#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "lru.h"

bool my_strcmp(void *a, void *b)
{
	if (strcmp((char *)a, (char *)b) == 0) {
		return true;
	}
	return false;
}

// a bad hash func
uint32_t my_str_hash(void *a)
{
	uint32_t hash = 0;
	char *str = (char *)a;
	while (*str) {
		hash += *str;
		str++;
	}
	return hash;
}

void my_free(void *key, void *data)
{
	free(data);
}

void my_print_item(void *a)
{
	char *str = (char *)a;
	printf("%s", str);
}

int main(void)
{
	struct lru_cache *cache = lru_init(2, my_str_hash, my_strcmp, my_strcmp, my_free);
	lru_insert(cache, "foo", strdup("bar"), true);
	lru_insert(cache, "bar", strdup("foo"), true);
	lru_dump(cache, my_print_item, my_print_item);
	lru_get(cache, "foo");
	lru_dump(cache, my_print_item, my_print_item);
	lru_insert(cache, "foobar", strdup("foobar"), true);
	lru_dump(cache, my_print_item, my_print_item);
	//hash collision
	lru_insert(cache, "barfoo", strdup("barfoo"), true);
	lru_dump(cache, my_print_item, my_print_item);
	lru_insert(cache, "deadbeef", strdup("deadbeef"), true);
	lru_dump(cache, my_print_item, my_print_item);
	lru_get(cache, "foo");
	lru_get(cache, "barfoo");
	lru_dump(cache, my_print_item, my_print_item);
	lru_insert(cache, "deadbeef", strdup("newdata"), true);
	lru_dump(cache, my_print_item, my_print_item);
	return 0;
}
