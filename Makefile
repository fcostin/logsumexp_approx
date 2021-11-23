all:	main
.PHONY: all


main:	main.c jit_logsumexp.c types.h jit_compare_tree.h
	./clangbot.sh clang-13 -Wall --std=gnu99 -g -march=native -O2 -o $@ $< -lm


jit_compare_tree.s:	scripts/compare_tree.py
	python3 $< > $@


jit_compare_tree.h:	scripts/stoh.py jit_compare_tree.s
	python3 scripts/stoh.py --in-file jit_compare_tree.s --out-file $@


clean:
	rm -f main
.PHONY: clean
