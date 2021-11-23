all:	main
.PHONY: all

main:	main.c jit_logsumexp.c types.h
	./clangbot.sh clang-13 -Wall --std=gnu99 -g -march=native -O2 -o $@ $< -lm

clean:
	rm -f main main_fast
.PHONY: clean
