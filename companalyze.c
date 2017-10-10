#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>

#include "lib/heuristic.h"

#define MMAP_PROT (PROT_READ|PROT_NONE)
#define MMAP_FLAG (MAP_SHARED)

int main(int argc, char *argv[]) {
	struct stat file_stat;
	clock_t start, end;
	uint64_t file_size;
	int64_t fd;
	void *addr;

	fd = open(argv[1], O_RDONLY);
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
	start = clock()*1000000/CLOCKS_PER_SEC;
	heuristic_stats(addr, file_size);
	end = clock()*1000000/CLOCKS_PER_SEC;

	printf("Perf: %lu us ~ %fMiB/s\n", (end - start), file_size*1.0/(end - start));

	return 0;
}
