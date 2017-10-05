#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "sort.h"
#include "heuristic.h"
#include "log2_lshift16.h"

static uint32_t count_pairs(struct heuristic_ws *ws, uint32_t start)
{
	uint32_t i;
	uint32_t pairs_count = 0;
	uint8_t pairs[2];
	uint8_t *addr;

	pairs[0] = ws->bucket[start].symbol;
	pairs[1] = ws->bucket[start+1].symbol;

	for (i = 0; i < ws->sample_size-1; i += 1 + addr - ws->sample) {
		addr = (uint8_t *) memchr(ws->sample+i, pairs[0], ws->sample_size-i-1);
		if (addr == NULL)
			break;
		if (ws->sample[i] == pairs[1])
			pairs_count++;
	}

	for (i = 0; i < ws->sample_size-1; i += 1 + addr - ws->sample) {
		addr = (uint8_t *) memchr(ws->sample+i, pairs[1], ws->sample_size-i-1);
		if (addr == NULL)
			break;
		if (ws->sample[i] & pairs[0])
			pairs_count++;
	}

	return pairs_count;
}

/* Pair distance from random distribution */
static uint32_t random_distribution_distance(struct heuristic_ws *ws, uint32_t coreset_size)
{
	uint32_t i, pairs_count;
	uint64_t sum = 0;
	int64_t buf[2];

	for (i = 0; i < coreset_size-1; i++) {
		pairs_count = count_pairs(ws, i);

		buf[0] = ws->bucket[i].count * ws->bucket[i+1].count;
		buf[0] = buf[0] << 17;
		buf[0] /= (ws->sample_size * ws->sample_size);

		buf[1] = pairs_count * 2 << 17;
		buf[1] = pairs_count / ws->sample_size;

		sum += (buf[0] - buf[1])*(buf[0] - buf[1]);
	}

	return sum >> 10;
}

/*
 * Shannon Entropy calculation
 * return % of maximum entropy
 * <70 -> try compress data
 * <90 -> try additional checks
 * >90 -> don't compress
 */

static uint32_t shannon_entropy_perc(struct heuristic_ws *ws)
{
	int64_t entropy_max = 8*LOG2_RET_SHIFT;
	uint64_t p, q, entropy_sum;
	uint32_t i;

	q = log2_lshift16(ws->sample_size);
	entropy_sum = 0;
	for (i = 0; i < BUCKET_SIZE && ws->bucket[i].count > 0; i++) {
		p = ws->bucket[i].count;
		entropy_sum += p*(q-log2_lshift16(p));
	}

	entropy_sum /= ws->sample_size;
	return entropy_sum * 100 / entropy_max;
}

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


/* Compare buckets by size, ascending */
static inline int bucket_comp_rev(const void *lv, const void *rv)
{
	const struct bucket_item *l = (struct bucket_item *)(lv);
	const struct bucket_item *r = (struct bucket_item *)(rv);

	return r->count - l->count;
}

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
	struct bucket_item *bucket = ws->bucket;

	/* Sort in reverse order */
	sort(bucket, BUCKET_SIZE, sizeof(*bucket), &bucket_comp_rev, NULL);

	for (i = 0; i < BUCKET_SIZE && bucket[i].count > 0; i++) {
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

static void __heuristic_stats(uint8_t *addr, long unsigned byte_size)
{
	long unsigned i, curr_sample_pos;
	struct heuristic_ws workspace;
	uint32_t byte_set = 0;
	uint32_t byte_core_set = 0;
	uint32_t shannon_e_i = 0, shannon_e_f = 0;
	uint32_t rnd_distribution_dist = 0;

	workspace.sample = (uint8_t *) malloc(MAX_SAMPLE_SIZE);
	workspace.bucket = (struct bucket_item *) calloc(BUCKET_SIZE, sizeof(*workspace.bucket));

	curr_sample_pos = 0;
	for (i = 0; i < byte_size; i+= SAMPLING_INTERVAL) {
		memcpy(&workspace.sample[curr_sample_pos], &addr[i], SAMPLING_READ_SIZE);
		curr_sample_pos += SAMPLING_READ_SIZE;
	}

	workspace.sample_size = curr_sample_pos;

	int reppat = sample_repeated_patterns(&workspace);

	memset(workspace.bucket, 0, sizeof(*workspace.bucket)*BUCKET_SIZE);

	for (i = 0; i < BUCKET_SIZE; i++)
		workspace.bucket->symbol = i;

	for (i = 0; i < workspace.sample_size; i++) {
		uint8_t *byte = &workspace.sample[i];
		workspace.bucket[byte[0]].count++;
	}

	byte_set = byte_set_size(&workspace);

	byte_core_set = byte_core_set_size(&workspace);

	shannon_e_i = shannon_entropy_perc(&workspace);
	shannon_e_f = shannon_f(&workspace);

	rnd_distribution_dist = random_distribution_distance(&workspace, byte_core_set);

	printf("BSize: %6lu, RepPattern: %i, BSet: %3u, BCSet: %3u, ShanEi%%:%3u|%3u, RndDist: %5u\n", byte_size, reppat, byte_set, byte_core_set, shannon_e_i, shannon_e_f, rnd_distribution_dist);

	free(workspace.sample);
	free(workspace.bucket);
}

void heuristic_stats(void *addr, long unsigned byte_size)
{
	uint64_t i;
	uint8_t *in_data = (uint8_t *) addr;
	long unsigned chunks = byte_size / BTRFS_MAX_UNCOMPRESSED;
	long unsigned tail   = byte_size % BTRFS_MAX_UNCOMPRESSED;

	for (i = 0; i < chunks; i++) {
		printf("%5lu. ", i);
		__heuristic_stats(in_data, BTRFS_MAX_UNCOMPRESSED);
		in_data += BTRFS_MAX_UNCOMPRESSED;
	}

	if (tail) {
		printf("%5lu. ", i);
		__heuristic_stats(in_data, tail);
	}
}
