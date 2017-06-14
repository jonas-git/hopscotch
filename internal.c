#define plog1(n) ((n) >= 2 ? 1 : 0)
#define plog2(n) ((n) >= 1 << 2 ? (2 + plog1((n) >> 2)) : plog1(n))
#define plog4(n) ((n) >= 1 << 4 ? (4 + plog2((n) >> 4)) : plog2(n))
#define plog8(n) ((n) >= 1 << 8 ? (8 + plog4((n) >> 8)) : plog4(n))
#define constexpr_log2(n) \
	((int)((n) >= 1 << 16 ? (16 + plog8((n) >> 16)) : plog8(n)))

struct map_key {
	const char *value;
	uint32_t hash;
};

#define map_create_key(key) \
	((struct map_key){ .value = (key), .hash = map_hash(key) })

static
int log2_64(uint64_t n)
{
	return constexpr_log2(n);
}

static inline
uint32_t next_pow2(uint32_t value)
{
	--value;
	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;
	return ++value;
}

static inline
uint32_t map_move_index(struct map *map, uint32_t index, int64_t num)
{
	return (num + index + (num < 0 && -num > index ? map->size : 0)) & map->mask;
}
