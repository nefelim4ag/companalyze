CFLAGS ?= -O2 -Wall -lm

default all: lib companalyze.out

lib: ## Build libs
	$(MAKE) -C lib

companalyze.o: companalyze.c
	$(CC) $(CFLAGS) -c $? -o $@

companalyze.out: companalyze.o lib/heuristic.o
	$(CC) $(CFLAGS) -o $@ $?

clean: ## Cleanup
	git clean -dfx

help: ## Show help
	@fgrep -h "##" $(MAKEFILE_LIST) | fgrep -v fgrep | sed -e 's/\\$$//' | sed -e 's/##/\t/'
