# Hippocrates ASPLOS '21 Artifact

This document describes the artifact for our ASPLOS '21 paper on Hippocrates, 
a symbolic-execution based approach for systematically finding bugs in 
persistent memory applications and libraries. The remainder of this document 
describes how to run Hippocrates and reproduce the key results from our paper.

### Resources

**Server login**: We will be providing access to one of our servers, which has
real NVDIMMs installed on it (this is required for our performance evaluation).
We will provide the login procedure

### Artifact Overview


`targets/`: This directory contains the scripts required to run all the tests needed to reproduce the main results from the paper.
After running experiments, the results will be placed into the `results/` directory

`results/`: This directory contains the scripts required to parse the results generated from the main experiments.

`vm_scripts/`: This directory contains scripts for building and running the evaluation VM.


## Artifacts Available Criteria

Hippocrates is open-source and is available at https://github.com/efeslab/hippocrates.git.


## Artifacts Functional Criteria

We now provide an overview of how to build and run Hippocrates. For a guide on how to compile applications to run on Hippocrates, see [Klee's tutorial on building coreutils](https://klee.github.io/tutorials/testing-coreutils/).


### Building Hippocrates

#### Installing Requirements (optional)

This step is only needed if setting up Hippocrates on a new machine.

```
# Requires root
./install-deps.sh
```


#### Compiling Hippocrates


```
make PMFIXER 
make PMINTRINSICS
```


### Building Dependencies

- wllvm:

```
cd deps/whole-program-llvm
sudo -H pip3 install -e . 
```

- pmemcheck:

```
make PMEMCHECK
```

- PMDK:

```
make PMDK
```

- Redis:

```
cd build
make REDIS

cd ../deps/redis/src
extract-bc redis-server
llvm-link-8 redis-server.bc --override=../../../build/deps/pmdk/lib/pmdk_debug/libpmem.so.bc --override=../../../build/deps/pmdk/lib/pmdk_debug/libpmemobj.so.bc -o redis-server.linked.bc

cd -
make PMFIXER && ./remove-flushes ../deps/redis/src/redis-server.linked.bc -o ../deps/redis/src/redis-server-noflush -s

make PMFIXER && make PMINTRINSICS && time ./apply-fixer ../deps/redis/src/redis-server-noflush.bc ../deps/redis/src/redis_noflush_112420.trace -o ../deps/redis/src/redis-server-trace --keep-files --cxx --extra-opt-args="-fix-summary-file=redis_heuristic_traceaa.txt -heuristic-raising -trace-aa" > log_trace.txt 2>&1

```

### Running Hippocrates

- Redis:

```
cd deps/redis/src
# Make the trace using pmemcheck
rm /mnt/pmem/redis.pmem; ~/workspace/pm-bug-fixing/build/deps/valgrind-pmem/bin/valgrind --tool=pmemcheck --log-file=redis_noflush.log ./redis-server-noflush ../../redis-pmem.conf

../../../build/parse-trace pmemcheck redis_noflush.log -o redis_noflush.trace

# Build the evaluation targets using the Hippocrates
cd build
time ./apply-fixer ../deps/redis/src/redis-server-noflush.bc ../deps/redis/src/redis_noflush.trace -o ../deps/redis/src/redis-server-trace --keep-files --cxx --extra-opt-args="-fix-summary-file=redis_heuristic_traceaa.txt -heuristic-raising -trace-aa"

make PMFIXER && ./apply-fixer ../deps/redis/src/redis-server.linked.bc redis_noflush.trace -o ../deps/redis/src/redis-server-dumb --keep-files --cxx --extra-opt-args="-fix-summary-file=redis_dumb_summary.txt -disable-raising -extra-dumb" 
```


## Results Reproduced

There are three main results from Hippocrates:

1. Fixing all previously reported bugs (§6.1)
2. The Redis performance experiment (§6.3) (Note: this requires access to real PM.)
3. Hippocrates's overhead (§6.4)

### 1. Fixing previously reported bugs.

```
cd build
./verify
```

The output should look like:

```
```

### 2. Performing the Redis experiment

[//]: # (Links below)