#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
/* For float log2() */
#include <math.h>

#include "sort.h"
#include "heuristic.h"

int enable_stats_printf;

#if (0)
/* I think that not working as expected */
/* Pair distance from random distribution */
static uint32_t random_distribution_distance(struct heuristic_ws *ws, uint32_t coreset_size)
{
	uint64_t sum = 0;
	int64_t buf[2];
	uint32_t i, i2;
	uint16_t pairs_count;
	uint8_t  pairs[2];
	uint8_t *sample = ws->sample;

	for (i = 0; i < coreset_size-1; i += 1) {
		pairs[0] = ws->bucket[i+0].symbol;
		pairs[1] = ws->bucket[i+1].symbol;

		pairs_count = 0;

		/*
		 * Search: ab, ba, bc, cb, cd, dc... pairs
		 */
		for (i2 = 0; i2 < ws->sample_size - 1; i2++) {
			if (sample[i2] == pairs[0] && sample[i2 + 1] == pairs[1]) {
				pairs_count++;
				continue;
			}
			if (sample[i2] == pairs[1] && sample[i2 + 1] == pairs[0]) {
				pairs_count++;
				continue;
			}
		}

		buf[0] = ws->bucket[i].count * ws->bucket[i+1].count;
		buf[0] = buf[0] << 17;
		buf[0] /= (ws->sample_size * ws->sample_size);

		buf[1] = pairs_count * 2 << 17;
		buf[1] = pairs_count / ws->sample_size;

		sum += (buf[0] - buf[1])*(buf[0] - buf[1]);
	}

	return sum >> 13;
}
#endif

/*
 * Shannon Entropy calculation
 *
 * Pure byte distribution analyze fail to determine
 * compressiability of data. Try calculate entropy to
 * estimate the average minimum number of bits needed
 * to encode a sample data.
 *
 * For comfort, use return of percentage of needed bit's,
 * instead of bit's amaount directly.
 *
 * In theory if heuristic hit that check,
 * then heuristic returns OK only compression,
 * with Huffman coding
 *
 * @ENTROPY_LVL_ACEPTABLE - below that threshold sample has low byte
 * entropy and can be compressible with high probability
 *
 * @ENTROPY_LVL_HIGH - data are not compressible with high probability,
 * if that not possible to get additional analyze, use that threshold
 * to restrict compression of data.
 */

/* Ex. of ilog2 */
static int ilog2(uint64_t v)
{
	int l = 0;
	while ((1UL << l) < v)
		l++;
	return l;
}

const int tab64[64] = {
    63,  0, 58,  1, 59, 47, 53,  2,
    60, 39, 48, 27, 54, 33, 42,  3,
    61, 51, 37, 40, 49, 18, 28, 20,
    55, 30, 34, 11, 43, 14, 22,  4,
    62, 57, 46, 52, 38, 26, 32, 41,
    50, 36, 17, 19, 29, 10, 13, 21,
    56, 45, 25, 31, 35, 16,  9, 12,
    44, 24, 15,  8, 23,  7,  6,  5};

int ilog2_64 (uint64_t value)
{
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;
    return tab64[((uint64_t)((value - (value >> 1))*0x07EDD5E59A4E28C2)) >> 58];
}


#define ENTROPY_LVL_ACEPTABLE 70
#define ENTROPY_LVL_HIGH 85

static uint32_t ilog2_w(uint64_t num)
{
#if (0)
	return ilog2(num*num*num*num);
#else
	return ilog2_64(num*num*num*num);
#endif
}

static uint32_t shannon_entropy(struct heuristic_ws *ws)
{
	const uint32_t entropy_max = 8*ilog2_w(2);
	uint32_t entropy_sum = 0;
	uint32_t p, p_base, sz_base;
	uint32_t i;

	sz_base = ilog2_w(ws->sample_size);
	for (i = 0; i < ws->bucket_size; i++) {
		p = ws->bucket[i].count;
		p_base = ilog2_w(p);
		entropy_sum += p*(sz_base-p_base);
	}

	entropy_sum /= ws->sample_size;
	return entropy_sum * 100 / entropy_max;
}

#if 0
static uint32_t shannon_f(struct heuristic_ws *ws) {
	uint32_t i;
	double entropy_sum = 0;
	double entropy_max = 8;

	for (i = 0; i < BUCKET_SIZE && ws->bucket[i].count > 0; i++) {
		double p = ws->bucket[i].count;
		p = p / ws->sample_size;
		entropy_sum += -(p) * log2(p);
	}

	return entropy_sum*100/entropy_max;
}
#endif

/* Compare buckets by size, ascending */
static inline int bucket_comp_rev(const void *lv, const void *rv)
{
	const struct bucket_item *l = (struct bucket_item *)(lv);
	const struct bucket_item *r = (struct bucket_item *)(rv);

	return r->count - l->count;
}

#define ALIGN_UP(x, align_to)	(((x) + ((align_to)-1)) & ~((align_to)-1))

#define USE_HEAP_SORT 1
#define RADIX_BASE 4
#define COUNTERS_SIZE (1 << RADIX_BASE)

#if (!USE_HEAP_SORT)
static inline uint8_t get4bits(uint64_t num, int shift) {
	uint8_t low4bits;
	num = num >> shift;
	/* Reverse order */
	low4bits = (COUNTERS_SIZE - 1) - (num % COUNTERS_SIZE);
	return low4bits;
}

static inline void copy_cell(void *dst, int dest_i, void *src, int src_i)
{
	struct bucket_item *dstv = (struct bucket_item *) dst;
	struct bucket_item *srcv = (struct bucket_item *) src;
	dstv[dest_i] = srcv[src_i];
}

static inline uint64_t get_num(const void *a, int i)
{
	struct bucket_item *av = (struct bucket_item *) a;
	return av[i].count;
}

/*
 * Kernel compatible radix sort implementation
 * Use 4 bits as radix base
 * Use 16 64bit counters for calculating new possition in buf array
 *
 * @array     - array that will be sorted
 * @array_buf - buffer array to store sorting results
 *              must be equal in size to @array
 * @num       - array size
 * @max_cell  - Link to element with maximum possible value
 *              that can be used to cap radix sort iterations
 *              if we know maximum value before call sort
 * @get_num   - function to extract number from array
 * @copy_cell - function to copy data from array to array_buf
 *              and vise versa
 * @get4bits  - function to get 4 bits from number at specified offset
 */

static void radix_sort(void *array, void *array_buf,
		       int num,
		       const void *max_cell,
		       uint64_t (*get_num)(const void *, int i),
		       void (*copy_cell)(void *dest, int dest_i,
					 void* src, int src_i),
		       uint8_t (*get4bits)(uint64_t num, int shift))
{
	uint64_t max_num;
	uint64_t buf_num;
	uint64_t counters[COUNTERS_SIZE];
	uint64_t new_addr;
	int i;
	int addr;
	int bitlen;
	int shift;

	/*
	 * Try avoid useless loop iterations
	 * For small numbers stored in big counters
	 * example: 48 33 4 ... in 64bit array
	 */
	if (!max_cell) {
		max_num = get_num(array, 0);
		for (i = 1; i < num; i++) {
			buf_num = get_num(array, i);
			if (buf_num > max_num)
				max_num = buf_num;
		}
	} else {
		max_num = get_num(max_cell, 0);
	}


	buf_num = ilog2(max_num);
	bitlen = ALIGN_UP(buf_num, RADIX_BASE*2);

	shift = 0;
	while (shift < bitlen) {
		memset(counters, 0, sizeof(counters));

		for (i = 0; i < num; i++) {
			buf_num = get_num(array, i);
			addr = get4bits(buf_num, shift);
			counters[addr]++;
		}

		for (i = 1; i < COUNTERS_SIZE; i++) {
			counters[i] += counters[i-1];
		}

		for (i = (num - 1); i >= 0; i --) {
			buf_num = get_num(array, i);
			addr = get4bits(buf_num, shift);
			counters[addr]--;
			new_addr = counters[addr];
			copy_cell(array_buf, new_addr, array, i);
		}

		shift += RADIX_BASE;

		/*
		 * For normal radix, that expected to
		 * move data from tmp array, to main.
		 * But that require some CPU time
		 * Avoid that by doing another sort iteration
		 * to origin array instead of memcpy()
		 */
		memset(counters, 0, sizeof(counters));

		for (i = 0; i < num; i ++) {
			buf_num = get_num(array_buf, i);
			addr = get4bits(buf_num, shift);
			counters[addr]++;
		}

		for (i = 1; i < COUNTERS_SIZE; i++) {
			counters[i] += counters[i-1];
		}

		for (i = (num - 1); i >= 0; i--) {
			buf_num = get_num(array_buf, i);
			addr = get4bits(buf_num, shift);
			counters[addr]--;
			new_addr = counters[addr];
			copy_cell(array, new_addr, array_buf, i);
		}

		shift += RADIX_BASE;
	}
}
#endif

#if (0)
static void bucket_radix_sort(const struct heuristic_ws *ws)
{
	uint16_t max_num = 0;
	uint16_t counters[COUNTERS_SIZE];
	uint8_t addr, new_addr, bitlen;
	int i, shift;

	for (i = 0; i < BUCKET_SIZE; i++) {
		if (ws->bucket[i].count > max_num)
			max_num = ws->bucket[i].count;
	}

	max_num = ilog2(max_num);
	bitlen = ALIGN_UP(max_num, 4);

	for (shift = 0; shift < bitlen; shift += 4) {
		memset(counters, 0, sizeof(counters));

		for (i = 0; i < BUCKET_SIZE; i++) {
			addr = get4bits(ws->bucket[i].count, shift);
			counters[addr]++;
		}

		for (i = 1; i < COUNTERS_SIZE; i++) {
			counters[i] += counters[i-1];
		}

		for (i = BUCKET_SIZE - 1; i >= 0; i--) {
			addr = get4bits(ws->bucket[i].count, shift);
			counters[addr]--;
			new_addr = counters[addr];
			ws->bucket_tmp[new_addr] = ws->bucket[i];
		}
		memcpy(ws->bucket, ws->bucket_tmp, BUCKET_SIZE*sizeof(*ws->bucket));
	}
}
#endif

/*
 * Byte Core set size
 * How many bytes use 90% of sample
 *
 * Several type of structure d binary data have in general
 * nearly all types of bytes, but distribution can be Uniform
 * where in bucket all byte types will have the nearly same count
 * (ex. Encrypted data)
 * and as ex. Normal (Gaussian), where count of bytes will be not so linear
 * in that case data can be compressible, probably compressible, and
 * not compressible, so assume:
 *
 * @BYTE_CORE_SET_LOW - main part of byte types repeated frequently
 *		      compression algo can easy fix that
 * @BYTE_CORE_SET_HIGH - data have Uniform distribution and with high
 *		       probability not compressible
 *
 */

#define BYTE_CORE_SET_LOW  64
#define BYTE_CORE_SET_HIGH 200

static int byte_core_set_size(struct heuristic_ws *ws)
{
	uint32_t i;
	uint32_t coreset_sum = 0;
	uint32_t core_set_threshold = ws->sample_size * 90 / 100;
	uint32_t coreset_end;
	struct bucket_item *bucket = ws->bucket;
	struct bucket_item max_cell;

	/* Sort in reverse order */
#if (USE_HEAP_SORT)
	sort(bucket, ws->bucket_size, sizeof(*bucket), &bucket_comp_rev, NULL);
#else
	max_cell.count = MAX_SAMPLE_SIZE;
	radix_sort(ws->bucket, ws->bucket_tmp,
		   ws->bucket_size, &max_cell,
		   get_num, copy_cell, get4bits);
#endif

	/*
	 * Pre find end of bucket items
	 */
	while (ws->bucket_size >= 0) {
		ws->bucket_size--;
		i = ws->bucket_size;
		if (bucket[i].count != 0)
			break;
	}

	for (i = 0; i < BYTE_CORE_SET_LOW; i++)
		coreset_sum += bucket[i].count;

	if (coreset_sum > core_set_threshold)
		return i;

	coreset_end = BYTE_CORE_SET_HIGH;
	if (coreset_end > ws->bucket_size)
		coreset_end = ws->bucket_size;

	for (; i < coreset_end; i++) {
		coreset_sum += bucket[i].count;
		if (coreset_sum > core_set_threshold)
			break;
	}

	return i;
}

static int byte_core_set_size_stats(struct heuristic_ws *ws)
{
	uint32_t i;
	uint32_t coreset_sum = 0;
	uint32_t core_set_threshold = ws->sample_size * 90 / 100;
	struct bucket_item *bucket = ws->bucket;
	struct bucket_item max_cell;

	/* Sort in reverse order */
#if (USE_HEAP_SORT)
	sort(bucket, ws->bucket_size, sizeof(*bucket), &bucket_comp_rev, NULL);
#else
	max_cell.count = MAX_SAMPLE_SIZE;
	radix_sort(ws->bucket, ws->bucket_tmp,
		   ws->bucket_size, &max_cell,
		   get_num, copy_cell, get4bits);
#endif
	/*
	 * Pre find end of bucket items
	 */
	while (ws->bucket_size >= 0) {
		ws->bucket_size--;
		i = ws->bucket_size;
		if (bucket[i].count != 0)
			break;
	}

	for (i = 0; i < ws->bucket_size; i++) {
		coreset_sum += bucket[i].count;
		if (coreset_sum > core_set_threshold) {
			i++;
			break;
		}
	}

	return i;
}

/*
 * Count byte types in bucket
 * That heuristic can detect text like data (configs, xml, json, html & etc)
 * Because in most text like data byte set are restricted to limit number
 * of possible characters, and that restriction in most cases
 * make data easy compressible.
 *
 * @BYTE_SET_THRESHOLD - assume that all data with that byte set size:
 *	less - compressible
 *	more - need additional analize
 */

#define BYTE_SET_THRESHOLD 64

static uint32_t byte_set_size(const struct heuristic_ws *ws)
{
	uint32_t i;
	uint32_t byte_set_size = 0;

	for (i = 0; i < BYTE_SET_THRESHOLD; i++) {
		if (ws->bucket[i].count > 0)
			byte_set_size++;
	}

	/*
	 * Continue collecting count of byte types in bucket
	 * If byte set size bigger then threshold
	 * That useless to continue, because for that data type
	 * detection technique fail
	 */
	for (; i < BUCKET_SIZE; i++) {
		if (ws->bucket[i].count > 0) {
			byte_set_size++;
			if (byte_set_size > BYTE_SET_THRESHOLD)
				return byte_set_size;
		}
	}

	return byte_set_size;
}

static uint32_t byte_set_size_stats(const struct heuristic_ws *ws)
{
	uint32_t i;
	uint32_t byte_set_size = 0;

	for (i = 0; i < BUCKET_SIZE; i++) {
		if (ws->bucket[i].count > 0)
			byte_set_size++;
	}

	return byte_set_size;
}

static int sample_repeated_patterns(struct heuristic_ws *ws)
{
	uint32_t half_of_sample = ws->sample_size / 2;
	uint8_t *p = ws->sample;

	return !memcmp(&p[0], &p[half_of_sample], half_of_sample);
}

static void __heuristic_stats(uint8_t *addr, long unsigned byte_size, struct heuristic_ws *workspace)
{
	long unsigned i, curr_sample_pos;
	int ret = 0;
	uint32_t byte_set = 0;
	uint32_t byte_core_set = 0;
	uint32_t shannon_e_i = 0, shannon_e_f = 0;
	uint32_t rnd_distribution_dist = 0;
	float error = 0;

	curr_sample_pos = 0;
	for (i = 0; i < byte_size; i+= SAMPLING_INTERVAL) {
		memcpy(&workspace->sample[curr_sample_pos], &addr[i], SAMPLING_READ_SIZE);
		curr_sample_pos += SAMPLING_READ_SIZE;
	}

	workspace->sample_size = curr_sample_pos;

	int reppat = sample_repeated_patterns(workspace);
	if (reppat)
		ret = 1;

	memset(workspace->bucket, 0, sizeof(*workspace->bucket)*BUCKET_SIZE);

	for (i = 0; i < workspace->sample_size; i++) {
		uint8_t byte = workspace->sample[i];
		workspace->bucket[byte].count++;
	}

	byte_set = byte_set_size_stats(workspace);
	if (byte_set <= BYTE_SET_THRESHOLD && ret == 0)
		ret = 2;

	workspace->bucket_size = BUCKET_SIZE;

#if (1)
	byte_core_set = byte_core_set_size_stats(workspace);
	if (byte_core_set <= BYTE_CORE_SET_LOW && ret == 0)
		ret = 3;

	if (byte_core_set >= BYTE_CORE_SET_HIGH && ret == 0)
		ret = -3;
#endif
	shannon_e_i = shannon_entropy(workspace);
#if(0)
	shannon_e_f = shannon_f(workspace);

	if (shannon_e_f > 0)
		error = 100 - (shannon_e_i * 1.0 * 100 / (shannon_e_f));
#endif

#if(0)
	rnd_distribution_dist = random_distribution_distance(&workspace, byte_core_set);
#endif
	if (shannon_e_i < ENTROPY_LVL_ACEPTABLE && ret == 0)
		ret = 4;
	else {
		if (shannon_e_i < ENTROPY_LVL_HIGH) {
			if (rnd_distribution_dist > 2) {
				if (ret == 0)
					ret = 5;
			} else {
				if (ret == 0)
					ret = 0;
			}
		} else {
			if (rnd_distribution_dist > 20) {
				if (ret == 0)
					ret = 6;
			} else {
				if (ret == 0)
					ret = 0;
			}
		}
	}

	if (enable_stats_printf) {
		printf("BSize: %6lu, RepPattern: %i, BSet: %3u, BCSet: %3u, ShanEi%%:%3u|%3u ~%3.1f%%, RndDist: %5u, out: %i\n",
			byte_size, reppat, byte_set, byte_core_set, shannon_e_i, shannon_e_f, error, rnd_distribution_dist, ret);
	}
}

void heuristic_stats(void *addr, long unsigned byte_size)
{
	uint64_t i;
	uint8_t *in_data = (uint8_t *) addr;
	struct heuristic_ws workspace;
	long unsigned chunks = byte_size / BTRFS_MAX_UNCOMPRESSED;
	long unsigned tail   = byte_size % BTRFS_MAX_UNCOMPRESSED;

	workspace.sample = (uint8_t *) malloc(MAX_SAMPLE_SIZE);
	workspace.bucket = (struct bucket_item *) calloc(BUCKET_SIZE*2, sizeof(*workspace.bucket));
	workspace.bucket_tmp = &workspace.bucket[BUCKET_SIZE];

	for (i = 0; i < chunks; i++) {
		if (enable_stats_printf) {
			printf("%5lu. ", i);
		}
		__heuristic_stats(in_data, BTRFS_MAX_UNCOMPRESSED, &workspace);
		in_data += BTRFS_MAX_UNCOMPRESSED;
	}

	if (tail) {
		if (enable_stats_printf) {
			printf("%5lu. ", i);
		}
		__heuristic_stats(in_data, tail, &workspace);
	}

	free(workspace.sample);
	free(workspace.bucket);
}

static void __heuristic(uint8_t *addr, long unsigned byte_size, struct heuristic_ws *workspace)
{
	long unsigned i, curr_sample_pos;
	int ret = 0;
	uint32_t byte_set = 0;
	uint32_t byte_core_set = 0;
	uint32_t shannon_e_i = 0;

	curr_sample_pos = 0;
	for (i = 0; i < byte_size; i+= SAMPLING_INTERVAL) {
		memcpy(&workspace->sample[curr_sample_pos], &addr[i], SAMPLING_READ_SIZE);
		curr_sample_pos += SAMPLING_READ_SIZE;
	}

	workspace->sample_size = curr_sample_pos;

	int reppat = sample_repeated_patterns(workspace);
	if (reppat) {
		ret = 1;
		goto out;
	}

	memset(workspace->bucket, 0, sizeof(*workspace->bucket)*BUCKET_SIZE);

	for (i = 0; i < workspace->sample_size; i++) {
		uint8_t *byte = &workspace->sample[i];
		workspace->bucket[byte[0]].count++;
	}

	byte_set = byte_set_size(workspace);
	if (byte_set <= BYTE_SET_THRESHOLD) {
		ret = 2;
		goto out;
	}

	workspace->bucket_size = BUCKET_SIZE;

	byte_core_set = byte_core_set_size(workspace);
	if (byte_core_set <= BYTE_CORE_SET_LOW) {
		ret = 3;
		goto out;
	}

	if (byte_core_set >= BYTE_CORE_SET_HIGH) {
		ret = -3;
		goto out;
	}

	shannon_e_i = shannon_entropy(workspace);

	if (shannon_e_i < ENTROPY_LVL_ACEPTABLE) {
		ret = 4;
		goto out;
	}

	if (shannon_e_i < ENTROPY_LVL_HIGH) {
		ret = 5;
		goto out;
	} else {
		ret = -5;
		goto out;
	}

out:
	if (enable_stats_printf) {
		printf("BSize: %6lu, RepPattern: %i, BSet: %3u, BCSet: %3u, ShanEi%%:%3u, out: %i\n",
			byte_size, reppat, byte_set, byte_core_set, shannon_e_i, ret);
	}
}

void heuristic(void *addr, long unsigned byte_size)
{
	uint64_t i;
	uint8_t *in_data = (uint8_t *) addr;
	struct heuristic_ws workspace;
	long unsigned chunks = byte_size / BTRFS_MAX_UNCOMPRESSED;
	long unsigned tail   = byte_size % BTRFS_MAX_UNCOMPRESSED;

	workspace.sample = (uint8_t *) malloc(MAX_SAMPLE_SIZE);
	workspace.bucket = (struct bucket_item *) calloc(BUCKET_SIZE*2, sizeof(*workspace.bucket));
	workspace.bucket_tmp = &workspace.bucket[BUCKET_SIZE];

	for (i = 0; i < chunks; i++) {
		if (enable_stats_printf) {
			printf("%5lu. ", i);
		}
		__heuristic(in_data, BTRFS_MAX_UNCOMPRESSED, &workspace);
		in_data += BTRFS_MAX_UNCOMPRESSED;
	}

	if (tail) {
		if (enable_stats_printf) {
			printf("%5lu. ", i);
		}
		__heuristic(in_data, tail, &workspace);
	}

	free(workspace.sample);
	free(workspace.bucket);
}
