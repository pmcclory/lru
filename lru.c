#include <stdio.h>
#include <stdlib.h>

#include "lru.h"

#define HT_BUCKETS 512

struct lru_cache_node {
	struct lru_cache_node *ll_prev;
	struct lru_cache_node *ll_next;
	struct lru_cache_node *ht_prev;
	struct lru_cache_node *ht_next;
	void *data;
	void *key;
};

struct lru_cache {
	uint32_t max_size;
	uint32_t cur_size;
	uint32_t (*hash_func)(void *);
	bool (*keycmp)(void *, void*);
	bool (*datacmp)(void *, void *);
	void (*freefunc)(void *, void *);
	struct lru_cache_node *ht[HT_BUCKETS];
	struct lru_cache_node *head;
	struct lru_cache_node *tail;
	struct lru_cache_node *free_nodes;
};

/*
 * "touch" a node - making it the most recently accessed
 */
static inline void touch(struct lru_cache *c, struct lru_cache_node *t)
{
	// believe this could be accomplished with a single tmp
	// but for the sake of clarity i'm allocating more
	struct lru_cache_node *p, *n;
	// short circuit - it's already the MRU
	if (t == c->head) {
		return;
	}
	p = t->ll_prev;
	n = t->ll_next;
	t->ll_next = c->head;
	t->ll_prev = NULL;
	if (n) {
		n->ll_prev = p;
	}
	if (p) {
		p->ll_next = n;
	}
	c->head->ll_prev = t;
	c->head = t;
	//if we touched the tail need to fix it
	if (c->tail == t) {
		c->tail = p;
	}
}

static inline void ht_remove(struct lru_cache *cache, struct lru_cache_node *node)
{
	uint32_t bucket = cache->hash_func(node->key) % HT_BUCKETS;
	if (node->ht_prev) {
		node->ht_prev->ht_next = node->ht_next;
	}
	if (node->ht_next) {
		node->ht_next->ht_prev = node->ht_prev;
	}
	if (cache->ht[bucket] == node) {
		cache->ht[bucket] = node->ht_next;
	}
	node->ht_prev = NULL;
	node->ht_next = NULL;
}

/*
 * evict the LRU - and return the now free node
 */
static struct lru_cache_node *evict(struct lru_cache *c)
{
	struct lru_cache_node *p = c->tail;

	/*
	 * remove from the main linked list
	 */
	if (p->ll_prev == NULL) {
		//this really shouldn't happen (unless the size of the cache is set to 1)	
		c->head = NULL;
		c->tail = NULL;
	} else {
		p->ll_prev->ll_next = NULL;
		c->tail = p->ll_prev;
	}

	// add it back to the head of the free list
	p->ll_next = c->free_nodes;
	p->ll_prev = NULL;
	if (c->free_nodes) {
		c->free_nodes->ll_prev = p;
	}
	c->free_nodes = p;
	c->cur_size -= 1;

	return p;	
}

/*
 * lease - move a node from the free list to the head of the
 * "in use" list
 */
static inline struct lru_cache_node *lease(struct lru_cache *cache)
{
	//head of the free node list
	struct lru_cache_node *r = cache->free_nodes;
	//remove it from the list
	cache->free_nodes = r->ll_next;
	if (cache->free_nodes) {
		cache->free_nodes->ll_prev = NULL;
	}
	r->ll_prev = NULL;

	//put it at the head of the cache linked list
	r->ll_next = cache->head;
	if (cache->head) {
		cache->head->ll_prev = r;
	}
	cache->head = r;
	cache->cur_size += 1;
	if (cache->tail == NULL) {
		cache->tail = r;
	}
	return r;
}

/*
 * put a node at the head of the ht linear chain
 */
static inline void ht_insert(struct lru_cache *cache, struct lru_cache_node *node)
{
	uint32_t bucket = cache->hash_func(node->key) % HT_BUCKETS;
	if (cache->ht[bucket] == NULL) {
		cache->ht[bucket] = node;
		node->ht_next = NULL;
		node->ht_prev = NULL;
		return;
	}
	node->ht_next = cache->ht[bucket];
	node->ht_prev = NULL;
	cache->ht[bucket]->ht_prev = node;
	cache->ht[bucket] = node;
}

static inline struct lru_cache_node *hash_get(struct lru_cache *c, void *key)
{
	struct lru_cache_node *p = NULL;
	uint32_t hash = c->hash_func(key) % HT_BUCKETS;
	if (c->ht[hash] == NULL) {
		return NULL;
	}
	p = c->ht[hash];
	while (p) {
		if (c->keycmp(p->key, key)) {
			return p;
		}
		p = p->ht_next;
	}
	return NULL;
}

uint32_t *default_str_hash(char *key)
{
	return NULL;
}

uint32_t *default_uint32_hash(uint32_t key)
{
	return NULL;
}

struct lru_cache *lru_init(uint32_t max_size,
			   uint32_t (*hashfunc)(void *),
			   bool (*keycmp)(void *, void *),
			   bool (*datacmp)(void *, void *),
			   void (*freefunc)(void *, void *))
{
	struct lru_cache *cache = calloc(1, sizeof(*cache));
	if (cache == NULL) {
		return NULL;
	}

	cache->max_size = max_size;
	cache->cur_size = 0;
	cache->hash_func = hashfunc;
	cache->keycmp = keycmp;
	cache->datacmp = datacmp;
	cache->freefunc = freefunc;

	struct lru_cache_node *nodes = calloc(max_size, sizeof(*nodes));
	if (nodes == NULL) {
		free(cache);
		return NULL;
	}
	for (int i = 0; i < max_size; i++) {
		if (i) {
			nodes[i].ll_prev = &nodes[i-1];
		} else {
			nodes[i].ll_prev = NULL;
		}
		if (i == (max_size - 1)) {
			nodes[i].ll_next = NULL;
		} else {
			nodes[i].ll_next = &nodes[i+1];
		}
		nodes[i].ht_next = NULL;
		nodes[i].ht_prev = NULL;
	}
	cache->free_nodes = nodes;
	return cache;
}

void *lru_get(struct lru_cache *cache, void *key)
{
	/*
	 * check for hashtable hit
	 */
	struct lru_cache_node *n = hash_get(cache, key);
	if (!n) {
		return NULL;
	}
	touch(cache, n);
	return n->data;
}

int lru_insert(struct lru_cache *cache, void *key, void *data, bool replace)
{
	/*
	 * check for hashtable hit
	 */
	struct lru_cache_node *n = hash_get(cache, key);
	if (n) {
		if (cache->datacmp(data, n->data)) {
			touch(cache, n);
		} else if (replace) {
			//call the user supplied freefunc to cleanup the old data
			cache->freefunc(n->key, n->data);
			n->data = data;
			n->key = key;
			touch(cache, n);
		} else {
			return -1;
		}
	} else {
		if (cache->cur_size == cache->max_size) {
			n = evict(cache);
			//remove this node from the hashtable
			ht_remove(cache, n);
			//call the user provided cleanup function
			cache->freefunc(n->key, n->data);
		}
		n = lease(cache);
		n->data = data;
		n->key = key;

		ht_insert(cache, n);
	}
	
	return 0;
}

void lru_dump(struct lru_cache *cache, void (*key_print)(void *), void (*data_print)(void *))
{
	printf("total entries: %d\n", cache->cur_size);
	struct lru_cache_node *p = cache->head;
	for (uint32_t i = 0; i < cache->cur_size; i++) {
		printf("\tEntry: %d\n", i);
		printf("\t\tKey: ");
		key_print(p->key);
		printf("\n");
		printf("\t\tData: ");
		data_print(p->data);
		printf("\n");
		p = p->ll_next;
	}

	for (uint32_t i = 0 ; i < HT_BUCKETS; i++) {
		if (cache->ht[i]) {
			printf("bucket[%u]:\n", i);
			p = cache->ht[i];
			while (p) {
				printf("\t\tKey: ");
				key_print(p->key);
				printf("\t\tData: ");
				data_print(p->data);
				printf("\n");
				p = p->ht_next;
			}
		}
	}
}
