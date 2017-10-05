CFLAGS = -O2 -Wall -lm

default all: lib log2_generator.out companalyze.out

lib: ## Build libs
	$(MAKE) -C lib


log2_generator.o: log2_generator.c
	$(CC) $(CFLAGS) -c $? -o $@

log2_generator.out: log2_generator.o
	$(CC) $(CFLAGS) -o $@ $?


companalyze.o: companalyze.c
	$(CC) $(CFLAGS) -c $? -o $@

companalyze.out: companalyze.o lib/heuristic.o lib/sort.o
	$(CC) $(CFLAGS) -o $@ $?

clean: ## Cleanup
	git clean -dfx

help: ## Show help
	@fgrep -h "##" $(MAKEFILE_LIST) | fgrep -v fgrep | sed -e 's/\\$$//' | sed -e 's/##/\t/'
