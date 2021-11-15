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

