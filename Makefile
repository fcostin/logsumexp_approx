all:	main
.PHONY: all

main:	main.c
	./clangbot.sh clang-13 -Wall --std=c99 -g -march=native -O2 -o $@ $^ -lm


# main_fast:	main.c
# 	./clangbot.sh clang-13 -Wall --std=c99 -g -march=native -O3 -ffast-math -fveclib=libmvec -o $@ $^ -lm


clean:
	rm -f main main_fast
.PHONY: clean
