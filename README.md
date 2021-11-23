log-sum-exp approximations
==========================


### configuring kernel to allow profiling

```
sudo sysctl -w kernel.perf_event_paranoid=-1
sudo sysctl -w kernel.kptr_restrict=0
```

### getting a version of clang that supports glibc libmvec vectorisation

need clang 12+
debian unstable has clang 13
can run it in a docker container

the resulting binary may be executed outside the container and profiled with `perf`.


results
-------

```
				running
branch		mode		time (s)
============	=============	=====		=================================================

master		base    	2.851   	log-sum-exp. glibc math.h exp and log

master		fast    	1.022   	replace exp with cheap approximation

master		faster  	0.750   	replace log with cheap approximation

sort-blocks     faster  	0.533   	sort blocks by width. reduces branch
						mispredictions from 7.59% to 0.02%

specialise-	fasterbb	0.421		specialise code to handle each fixed
block-length					block width from 1 to 10.

compile-blocks  gen     	0.350   	code generate a single function taking
						all block offsets and widths as known
						as compile time. note: around 20% of
						blocks are duplicated in input, clang
						finds all of these and factors them out.
						Compilation takes about 5 minutes.

jit_poc		jit		0.425 -- 0.468	code generate a single branchless function
						taking all block offsets and widths as
						known at runtime. Compile and execute at
						runtime using a crude JIT compiler.
						Cause of timing variability unknown, but
						perf stat -dd suggests that the size of the
						generated code may be placing a strain on
						the instruction cache. For larger inputs
						that trigger correspondingly larger
						generated code, performance can plummet.
```
