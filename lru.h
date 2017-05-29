#ifndef _LRU_H
#define _LRU_H

#include <stdint.h>
#include <stdbool.h>

struct lru_cache;

uint32_t *default_str_hash(char *key);
uint32_t *default_uint32_hash(uint32_t key);

struct lru_cache *lru_init(uint32_t max_size,
			   uint32_t (*hashfunc)(void *),
			   bool (*keycmp)(void *, void*),
			   bool (*datacmp)(void *, void *),
			   void (*freefunc)(void *, void *));

void *lru_get(struct lru_cache *, void *key);

int lru_insert(struct lru_cache *, void *key, void *data, bool replace);

void lru_dump(struct lru_cache *cache, void (*key_print)(void *), void (*data_print)(void *));

#endif
