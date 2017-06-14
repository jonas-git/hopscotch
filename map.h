#ifndef MAP_H
#define MAP_H

#include <stddef.h>
#include <stdint.h>

/* Initial size of a map. Must be greater or equal to MAP_NHD_LEN */
#define MAP_INIT_SIZE 16u
#define MAP_INIT_LOAD_FACTOR 0.98f

/* Length of the neighbourhood. */
#define MAP_NHD_LEN 8u
/* Bit of the outermost neighbour. */
#define MAP_NHD_OUTER 1
/* All hop information bits turned on. */
#define MAP_NHD_MASK_ALL (~(~0u << MAP_NHD_LEN))
/* The bit in the hop information that corresponds to a bucket itself. */
#define MAP_NHD_MASK_SELF map_nhd_mask(0)

/* The n-th bit in the hop information (from the left). */
#define map_nhd_mask(n) (1 << (MAP_NHD_LEN - 1 - (n)))
/* Checks if the neighbour at index is set. */
#define map_nbh_isset(hop_info, index) ((hop_info) & map_nhd_mask(index))
/* Checks if all neighbours are occupied. */
#define map_nbh_full(hop_info) (!(~(hop_info) & MAP_NHD_MASK_ALL))
/* Converts a key's hash into an index. */
#define map_hash_index(map, hash) ((hash) & (map)->mask)
/* Creates a mask for map_hash_index() to replace the modulo operation. */
#define map_create_mask(size) (~(~0u << log2_64(size)))
/* Checks if a bucket is empty. */
#define map_bucket_empty(bucket) (!(bucket)->key)
/* Gets the next index in a map's bucket array. */
#define map_next_index(map, i) (((i) + 1) & (map)->mask)
/* Grows the map to the next power of 2. */
#define map_grow(map) map_resize(map, (map)->size | 1)

typedef int value_t;

struct map_bucket {
	const char *key;
	value_t value;
	unsigned int hop_info : MAP_NHD_LEN;
};

struct map {
	struct map_bucket *buckets;
	uint32_t count;
	uint32_t size;
	uint32_t mask; /* uint8_t */
	float load_factor;
};

uint32_t map_hash(const char *str);

struct map *map_new(void);
void map_free(struct map *map);

int map_resize(struct map *map, uint32_t size);
int map_set(struct map *map, const char *key, value_t value);
int map_get(struct map *map, const char *key, value_t *out_value);
int map_delete(struct map *map, const char *key, value_t *out_value);

#endif /* MAP_H */
