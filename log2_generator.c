#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "lib/log2_lshift16.h"

/*
 * Btrfs use 128Kb max block max_input_size for compress data
 * And with our accuracy it's useless to increase this value
 */
const uint32_t max_input_size = 8*1024+512;
const uint32_t log_return_mpl = LOG2_RET_SHIFT;

int main(int argc, char *argv[]){
	int32_t old_log_val = 0;
	if (argc < 2) {
		printf("%s <0|1>\n", argv[0]);
		return 1;
	}
	bool gen = atoi(argv[1]);
	uint32_t i;
	uint32_t fraction_old = 0;
	if (gen) {
		printf("#include \"log2_lshift16.h\"\n\n");
		printf("/*\n");
		printf(" * Precalculated log2 values\n");
		printf(" * Shifting used for avoiding floating point\n");
		printf(" * Fraction must be left shifted by 16\n");
		printf(" * Return of log are left shifted by 3\n");
		printf(" */\n");
		printf("int log2_lshift16(long long unsigned x){\n");
		for (i = 1; i <= max_input_size; i++) {
			int32_t new_log_val = log2(i)*log_return_mpl;

			if (old_log_val != new_log_val) {
				if (i != fraction_old) {
					printf("\tif (x < %u)\n", i);
					printf("\t\treturn %i;\n", old_log_val);
					fraction_old = i;
				}
				old_log_val = new_log_val;
				if (old_log_val == 0)
					break;
			}
		}
		printf("\treturn 0;\n");
		printf("}\n");
	} else {
		printf("fraction fraction log2() == log2_lshift16()\n");
		for (i = 1; i < max_input_size; i++) {
			double fraction = (double) i;
			int32_t new_log_val = log2(fraction)*log_return_mpl;
			if (old_log_val != new_log_val) {
				old_log_val = new_log_val;
				printf("%6i %f %4i == %4i\n", i, fraction, new_log_val, log2_lshift16(fraction));
				if (old_log_val == 0)
					break;
			}
		}
	}
}
