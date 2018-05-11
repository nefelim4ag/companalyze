CFLAGS ?= -O2 -Wall -lm

objects := companalyze.o lib/heuristic.o

default all: companalyze.out

lib/heuristic.o: lib/heuristic.c  lib/heuristic.h
	$(CC) $(CFLAGS) -c $< -o $@

companalyze.o: companalyze.c lib/heuristic.h
	$(CC) $(CFLAGS) -c $< -o $@

companalyze.out: $(objects)
	$(CC) $(CFLAGS) -o $@ $^

clean: ## Cleanup
	git clean -dfx

help: ## Show help
	@fgrep -h "##" $(MAKEFILE_LIST) | fgrep -v fgrep | sed -e 's/\\$$//' | sed -e 's/##/\t/'
