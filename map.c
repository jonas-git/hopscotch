#include <stdlib.h>
#include <string.h>
#include "map.h"
#include "internal.c"

uint32_t map_hash(const char *str)
{
	uint32_t h = 2166136261;
	unsigned char c;
	while ((c = *str++))
		h ^= c, h *= 16777619;
	return h;
}

struct map *map_new(void)
{
	struct map *map = malloc(sizeof(struct map));
	if (!map)
		goto err_map;
	map->size = MAP_INIT_SIZE;
	map->buckets = calloc(map->size, sizeof(struct map_bucket));
	if (!map->buckets)
		goto err_buckets;
	map->load_factor = MAP_INIT_LOAD_FACTOR;
	map->mask = map_create_mask(map->size);
	map->count = 0;
	return map;

err_buckets:
	free(map);
err_map:
	return NULL;
}

void map_free(struct map *map)
{
	free(map->buckets);
	free(map);
}

int map_resize(struct map *map, uint32_t size)
{
	if (size <= map->size)
		return 1;

	uint32_t old_size = map->size;
	struct map_bucket *old_buckets = map->buckets;

	map->count = 0;
	map->size = size = next_pow2(size);
	map->mask = map_create_mask(map->size);
	map->buckets = calloc(size, sizeof(struct map_bucket));
	if (!map->buckets)
		goto err_buckets;

	/* Is there a way to improve this? */
	for (uint32_t i = 0; i < old_size; ++i)
		if (old_buckets[i].key)
			map_set(map, old_buckets[i].key, old_buckets[i].value);

	free(old_buckets);
	return 1;

err_buckets:
	map->buckets = old_buckets;
	return 0;
}

static
int map_internal_get(struct map *map, struct map_key *key,
                     struct map_bucket **out_bucket)
{
	uint32_t index = map_hash_index(map, key->hash);
	unsigned int hop_info = map->buckets[index].hop_info;
	for (unsigned int offset = 0; offset < MAP_NHD_LEN; ++offset) {
		if (!map_nbh_isset(hop_info, offset))
			continue;
		uint32_t offset_index = map_move_index(map, index, offset);
		struct map_bucket *bucket = map->buckets + offset_index;
		if (!bucket->key || strcmp(bucket->key, key->value) != 0)
			continue;
		*out_bucket = bucket;
		return 1;
	}
	return 0;
}

int map_get(struct map *map, const char *key, value_t *out_value)
{
	struct map_key mkey = map_create_key(key);
	struct map_bucket *bucket = NULL;
	if (!map_internal_get(map, &mkey, &bucket))
		return 0;
	*out_value = bucket->value;
	return 1;
}

static
int map_internal_swap(struct map *map, uint32_t *index_ptr,
                      uint32_t *distance_ptr)
{
	uint32_t move_index = map_move_index(map, *index_ptr, -MAP_NHD_LEN + 1);
	for (unsigned int distance = MAP_NHD_LEN - 1; distance; --distance) {
		struct map_bucket *bucket = map->buckets + move_index;

		int offset = -1;
		unsigned int mask = MAP_NHD_MASK_SELF;
		unsigned int hop_info = bucket->hop_info;
		for (unsigned int i = 0; i < distance; ++i, mask >>= 1)
			if (hop_info & mask) {
				offset = i;
				break;
			}

		if (offset >= 0) {
			struct map_bucket *target = map->buckets + *index_ptr;
			struct map_bucket *swap_bucket = bucket + offset;
			bucket->hop_info &= ~map_nhd_mask(offset);
			bucket->hop_info |= map_nhd_mask(distance);
			target->key = swap_bucket->key;
			target->value = swap_bucket->value;
			*index_ptr = swap_bucket - map->buckets;
			*distance_ptr -= (distance - offset);
			return 1;
		}

		move_index = map_next_index(map, move_index);
	}

	return 0;
}

int map_set(struct map *map, const char *key, value_t value)
{
	if (map->count >= map->size * map->load_factor)
		if (!map_resize(map, map->size | 1))
			return 0;

	struct map_key mkey = map_create_key(key);
	uint32_t index = map_hash_index(map, mkey.hash);

	/* Overwrite the bucket if the key was already set before. */
	struct map_bucket *bucket = NULL;
	if (map_internal_get(map, &mkey, &bucket)) {
		bucket->key = key;
		bucket->value = value;
		++map->count;
		return 1;
	}

	/* Rehash the map if the desired neighbourhood is full. */
	if (map_nbh_full(map->buckets[index].hop_info))
		return map_grow(map) ? map_set(map, key, value) : 0;

	/* Probe until we find an empty bucket. */
	uint32_t distance = 0;
	uint32_t probe_index = index;
	while (!map_bucket_empty(map->buckets + probe_index))
		probe_index = map_next_index(map, probe_index), ++distance;

	while (1) {
		/* This bucket is appropriate, when it is in the neighbourhood. */
		if (distance < MAP_NHD_LEN) {
			map->buckets[index].hop_info |= map_nhd_mask(distance);
			map->buckets[probe_index].key = key;
			map->buckets[probe_index].value = value;
			++map->count;
			return 1;
		}
		/* Swap the bucket with a nearer bucket. */
		if (!map_internal_swap(map, &probe_index, &distance))
			return map_grow(map) ? map_set(map, key, value) : 0;
	}
}

int map_delete(struct map *map, const char *key, value_t *out_value);
