all:	main	main_fast
.PHONY: all

main:	main.c
	./clangbot.sh clang-13 -Wall --std=c99 -march=native -O2 -o $@ $^ -lm


main_fast:	main.c
	./clangbot.sh clang-13 -Wall --std=c99 -march=native -O3 -ffast-math -fveclib=libmvec -o $@ $^ -lm

