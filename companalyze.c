#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>
#include <getopt.h>

#include "lib/heuristic.h"

#define MMAP_PROT (PROT_READ|PROT_NONE)
#define MMAP_FLAG (MAP_SHARED|MAP_NORESERVE)

extern int enable_stats_printf;

void show_help() {
	printf("Usage: companalyze -Ps -f <file>\n");
	printf("\t-P\t--printf-stats\t- print stats per 128KiB input data\n");
	printf("\t-s\t--stats-mode\t- make full calculation instead of heuristic logic emulation\n");
	printf("\t-f\t--file\t- set file for analyze\n");
	exit(0);
}

int main(int argc, char *argv[]) {
	struct stat file_stat;
	clock_t start, end;
	uint64_t file_size;
	int64_t fd;
	void *addr;

	static char file_path[4*1024];

	int stats_mode = 0;


	// Parse options
	int c;
	while (1) {
		int option_index = 0;
		const static struct option long_options[] = {
			{ "printf-stats",   no_argument, NULL, 'P' },
			{ "stats-mode",     no_argument, NULL, 's' },
			{ "file",     required_argument, NULL, 'f' },
			{ "help",           no_argument, NULL, 'h' }
		};

		c = getopt_long(argc, argv, "Pshf:", long_options, &option_index);
		if (-1 == c) {
			break;
		}

		switch (c) {
			case 'P':
				enable_stats_printf = 1;
				break;
			case 's':
				stats_mode = 1;
				break;
			case 'f':
				strcpy(file_path, optarg);
				break;
			case 'h':
				show_help(); // fallthrough
			default:
				return 2;
		}
	}

	fd = open(file_path, O_RDONLY);
	if (fd == -1) {
		printf("Can't open file: %s\n", argv[1]);
		return -1;
	}

	{
		/* Get the size of the file. */
		fstat (fd, &file_stat);
		file_size = file_stat.st_size;
	}

	addr = mmap (0, file_size, MMAP_PROT, MMAP_FLAG, fd, 0);
	enable_stats_printf = 0;
	start = clock()*1000000/CLOCKS_PER_SEC;
	if (stats_mode) {
		heuristic_stats(addr, file_size);
	} else {
		heuristic(addr, file_size);
	}
	end = clock()*1000000/CLOCKS_PER_SEC;

	munmap(addr, file_size);

	printf("Perf: %lu us ~ %.2f MiB/s\n", (end - start), file_size*1.0/(end - start));

	return 0;
}
