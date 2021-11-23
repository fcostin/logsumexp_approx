
all:	poc
.PHONY: poc

%.o:	%.s
	gcc -Wall -c $< -o $@

poc:	poc.c faster_log_exp_sum_bb.o
	gcc -Wall --std=gnu99 -o $@ $^ -lm
