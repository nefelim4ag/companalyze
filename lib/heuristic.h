#include <stdint.h>

/*
 * Btrfs compression restriction
 */
#define BTRFS_MAX_UNCOMPRESSED (128*1024)

/*
 * Heuristic use systematic sampling to collect data from
 * input data range, so some constant needed to control algo
 *
 * @SAMPLING_READ_SIZE - how many bytes will be copied from on each sample
 * @SAMPLING_INTERVAL  - period that used to iterate over input data range
 */
#define SAMPLING_READ_SIZE 16
#define SAMPLING_INTERVAL 256

/*
 * For statistic analize of input data range
 * consider that data consists from bytes
 * so this is Galois Field with 256 objects
 * each object have attribute count, i.e. how many times
 * that object detected in sample
 */
#define BUCKET_SIZE 256

/*
 * The size of the sample is based on a statistical sampling rule of thumb.
 * That common to perform sampling tests as long as number of elements in
 * each cell is at least five.
 *
 * Instead of five, for now choose 32 value to obtain more accurate results.
 * If the data contains the maximum number of symbols, which is 256,
 * lets obtain a sample size bound of 8192.
 *
 * So sample at most 8KB of data per data range: 16 consecutive
 * bytes from up to 512 locations.
 */
#define MAX_SAMPLE_SIZE (BTRFS_MAX_UNCOMPRESSED * \
			  SAMPLING_READ_SIZE / SAMPLING_INTERVAL)

struct bucket_item {
	uint32_t symbol;
	uint32_t count;
};

struct heuristic_ws {
	/* Partial copy of input data */
	uint8_t *sample;
	uint32_t sample_size;
	/* Bucket store counter for each byte type */
	struct bucket_item *bucket;
};

void heuristic_stats(void *addr, long unsigned byte_size);
