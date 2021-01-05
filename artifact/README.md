# Hippocrates ASPLOS '21 Artifact

This document describes the artifact for our ASPLOS '21 paper on Hippocrates, 
an automated tool for fixing PM durability bugs. The remainder of this document 
describes how to run Hippocrates and reproduce the key results from our paper.

### Resources

**Server login**: We will be providing access to one of our servers, which has
real NVDIMMs installed on it (this is required for our performance evaluation).
We will provide the login procedure on the artifact submission site.


## Artifacts Available Criteria

Hippocrates is open-source and is available at https://github.com/efeslab/hippocrates.git.


## Artifacts Functional Criteria

We now provide an overview of how to build and run Hippocrates. We provide pre-built binaries in `artifact/prebuilt` as an alternative to 
the following compilation instructions.

### Building Hippocrates

#### Installing Requirements (optional)

This step is only needed if setting up Hippocrates on a new machine.

```
# Requires root
./install-deps.sh
```

#### Setting up the repository (once)

After cloning the repository, you need to set up the submodules, which Hippocrates
depends on for both the LLVM pass and for running tests.

```
git clone https://github.com/efeslab/hippocrates.git
cd hippocrates

git submodule init
git submodule update --recursive
```

If you need to re-initialize the submodules for any reason, try:

```
git submodule deinit --all -f
git submodule init
git submodule update --recursive
```

#### Compiling Hippocrates

Starting in the root of the repository:

```
mkdir build
cd build
cmake ..

make PMFIXER -j$(nproc)
make PMINTRINSICS
```

### Building Dependencies

- wllvm (**optional**, only if installing on a new machine):

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

cd ./deps/pmdk/lib/pmdk_debug
extract-bc libpmem.so
extract-bc libpmemobj.so
```

- memcached-pmem

```
source build.env
cd build
make MEMCACHED_PMEM

cd ./deps/memcached-pmem/bin/
llvm-link-8 memcached.bc -o memcached.linked.bc ../../pmdk/lib/pmdk_debug/libpmem.so.bc
```

- RECIPE (P-CLHT)

```
cd build
make p-clht_example

cd ./deps/RECIPE/P-CLHT/
llvm-link-8 p-clht_example.bc --override=../../pmdk/lib/pmdk_debug/libpmem.so.bc \
        --override=../../pmdk/lib/pmdk_debug/libpmemobj.so.bc -o p-clht_example.linked.bc
```

- Redis:

```
cd build
make REDIS

cd ../deps/redis/src
extract-bc redis-server
llvm-link-8 redis-server.bc --override=../../../build/deps/pmdk/lib/pmdk_debug/libpmem.so.bc \
        --override=../../../build/deps/pmdk/lib/pmdk_debug/libpmemobj.so.bc -o redis-server.linked.bc

# This creates the baseline for the redis experiment
cd -
./remove-flushes ../deps/redis/src/redis-server.linked.bc -o ../deps/redis/src/redis-server-noflush -s
```

The following will be run in two terminals (to collect the trace):

- In the first terminal:
```
cd ./deps/redis/src/
rm /mnt/pmem/redis.pmem
~/workspace/pm-bug-fixing/build/deps/valgrind-pmem/bin/valgrind --tool=pmemcheck \
        --log-file=redis.log ./redis-server-noflush ../../redis-pmem.conf
```

- In the second terminal:
```
telnet localhost 6380
> set foo bar
...
> shutdown
```

After collecting the trace:

```
./parse-trace pmemcheck ../deps/redis/src/redis.log -o ../deps/redis/src/redis.trace

./apply-fixer ../deps/redis/src/redis-server-noflush.bc ../deps/redis/src/redis_noflush.trace -o ../deps/redis/src/redis-server-trace --keep-files --cxx --extra-opt-args="-fix-summary-file=redis_summary.txt -heuristic-raising -trace-aa"

./apply-fixer ../deps/redis/src/redis-server-noflush.bc redis_noflush.trace -o ../deps/redis/src/redis-server-dumb --keep-files --cxx --extra-opt-args="-fix-summary-file=redis_dumb_summary.txt -disable-raising -extra-dumb" 

```


## Results Reproduced

There are three main results from Hippocrates:

1. Fixing all previously reported bugs (§6.1)
2. The Redis performance experiment (§6.3) (Note: this requires access to real PM.)
3. Hippocrates's overhead (§6.4)

### 1. Fixing previously reported bugs.

#### PMDK bugs

We provide a script, `build/verify`, which does the following:

1. Gathers the list of all reproducible bugs listed
2. Patches PMDK to the correct version for each test
3. Builds the tests
4. Runs each test, generating a pmemcheck trace
5. Fixes each test based on the pmemcheck trace
6. Reruns each test and confirms that the bugs are no longer present.

**Note**: It is expected that the full test suite takes several hours to run.

To run this script, do the following:

```
cd build
./verify
```

The output should look like:

```
...
11 issues resolved:
[447, 452, 458, 459, 460, 461, 585, 940, 942, 943, 945]
```

If you instead want to run each issue individually, run the following:
```
# Issue 447
./verify --target obj_toid_TEST0_8bbb0af9c

# Issue 452
./verify --target obj_constructor_TEST0_8bbb0af9c
./verify --target obj_constructor_TEST2_8bbb0af9c

# Issue 458
./verify --target pmemspoil_TEST0_8bbb0af9c

# Issue 459
./verify --target pmem_memcpy_TEST0_8bbb0af9c
./verify --target pmem_memcpy_TEST1_8bbb0af9c
./verify --target pmem_memcpy_TEST2_8bbb0af9c
./verify --target pmem_memcpy_TEST3_8bbb0af9c

# Issue 460
./verify --target pmem_memmove_TEST0_0fd509d73
./verify --target pmem_memmove_TEST1_0fd509d73
./verify --target pmem_memmove_TEST2_0fd509d73
./verify --target pmem_memmove_TEST3_0fd509d73
./verify --target pmem_memmove_TEST4_0fd509d73
./verify --target pmem_memmove_TEST5_0fd509d73
./verify --target pmem_memmove_TEST6_0fd509d73
./verify --target pmem_memmove_TEST7_0fd509d73
./verify --target pmem_memmove_TEST8_0fd509d73
./verify --target pmem_memmove_TEST9_0fd509d73
./verify --target pmem_memmove_TEST10_0fd509d73
./verify --target pmem_memmove_TEST11_0fd509d73

# Issue 461
./verify --target pmem_memset_TEST1_0fd509d73

# Issue 585
./verify --target rpmemd_db_TEST0_60e24d2

# Issue 940
./verify --target obj_first_next_TEST0_e71dfa41b
./verify --target obj_first_next_TEST1_e71dfa41b

# Issue 942
./verify --target obj_mem_TEST0_e71dfa41b

# Issue 943
./verify --target obj_memops_TEST0_e71dfa41b

# Issue 945
./verify --target pmem_memset_TEST0_e71dfa41b
```


#### RECIPE bugs

To reproduce the RECIPE bugs, do the following:

1. First, generate the bug report using pmemcheck:
```
~/workspace/pm-bug-fixing/build/deps/valgrind-pmem/bin/valgrind --tool=pmemcheck --log-file=recipe.log ./deps/RECIPE/P-CLHT/p-clht_example 100 1
~/workspace/pm-bug-fixing/build/parse-trace pmemcheck recipe.log -o recipe.trace
```

2. Apply Hippocrates to fix the bugs:
```
cd build

./apply-fixer ./deps/RECIPE/P-CLHT/p-clht_example.linked.bc recipe.trace -o ./deps/RECIPE/P-CLHT/fixed --cxx --extra-opt-args="-fix-summary-file=recipe_summary.txt -heuristic-raising -trace-aa"
```

3. Rerun pmemcheck and generate a new bug report:
```
rm -f /mnt/pmem/pool 
~/workspace/pm-bug-fixing/build/deps/valgrind-pmem/bin/valgrind --tool=pmemcheck --log-file=recipe_fixed.log ./deps/RECIPE/P-CLHT/fixed 100 1
```

4. Confirm the bug report is empty, thus confirming the bugs are fixed:
```
~/workspace/pm-bug-fixing/build/parse-trace pmemcheck recipe_fixed.log -o recipe_fixed.trace
```

This should produce output similar to the following:
```
Identified trace start


Identified trace end
...
Prepare to dump.
        Num items: 1
Prepare to write.
Report written to recipe_fixed.trace
```

#### memcached-pmem bugs

To automate the below process, do the following:
```
cd build
./verify-memcached --help
./verify-memcached 
```

The `verify-memcached` can also run over the prebuilt files present in `artifact/prebuilt`
if you run using the `--use-prebuilt` flag.

To reproduce the memcached-pm bugs manually, do the following:

1. First, find the bugs and generate a pmemcheck trace:

```
cd build/deps/memcached/bin
LD_LIBRARY_PATH=$(realpath ../../pmdk/lib/pmdk_debug) ~/workspace/pm-bug-fixing/build/deps/valgrind-pmem/bin/valgrind --tool=pmemcheck --log-file=memcached.log ./memcached -m 0 -U 0 -t 1 -A -o pslab_file=/mnt/pmem/pool,pslab_size=8,pslab_force
```

In a second terminal:
```
telnet localhost 11211
> set foo 1 0 3
> bar
> shutdown
```

Then parse the trace:
```
cd build
./parse-trace pmemcheck ./deps/memcached/bin/memcached.log -o ./deps/memcached/bin/memcached.trace
```

2. Then, apply Hippocrates:
```
./apply-fixer ./deps/memcached-pmem/bin/memcached.linked.bc ./deps/memcached-pmem/bin/memcached.trace -o ./deps/memcached-pmem/bin/memcached-fixed --keep-files --extra-opt-args="-fix-summary-file=memcached_summary_time.txt -heuristic-raising -trace-aa" 
```

3. Repeat steps 1, except using the fixed binary. Then confirm the new binary does not have any bugs:
```
cd build/deps/memcached/bin
LD_LIBRARY_PATH=$(realpath ../../pmdk/lib/pmdk_debug) ~/workspace/pm-bug-fixing/build/deps/valgrind-pmem/bin/valgrind --tool=pmemcheck --log-file=memcached_fixed.log ./memcached-fixed -m 0 -U 0 -t 1 -A -o pslab_file=/mnt/pmem/pool,pslab_size=8,pslab_force

# --- Follow the same steps in a second terminal

cd build
./parse-trace pmemcheck ./deps/memcached/bin/memcached_fixed.log -o ./deps/memcached/bin/memcached_fixed.trace
```

The output should be like the following:

```
Identified trace start


Identified trace end
...
Prepare to dump.
        Num items: 1
Prepare to write.
Report written to memcached_fixed.trace
```

### 2. Performing the Redis experiment

1. First, we need to generate the baseline and the log.

```
cd deps/redis/src

```

Now, run the performance evaluation:

```
cd build
./run-redis -t 10 --output-file ../results/test.csv
# if using prebuilt:
./run-redis -t 10 --output-file ../results/test.csv -r ../artifact/prebuilt

cd ../results
./graph.py test.csv test.pdf
```

This should create a graph similar to Figure 4 in the paper, and should also produce textual output similar to the following:

```
          Redis$_{H-intra}$  Redis-pmem  Redis$_{H-full}$
Workload                                                 
Load               0.215850         1.0          1.003542
A                  0.797220         1.0          1.009079
B                  0.721691         1.0          1.029106
C                  0.728584         1.0          1.034976
D                  0.682358         1.0          1.004612
E                  0.167548         1.0          1.027780
F                  0.680404         1.0          0.997616
```


### 3. Hippocrates's overhead

This information is gathered from examining `./build/apply_fixer.log`. This is a log which is appended to for every invocation of the `apply-fixer` script.

The format of entries in this log is the following:

<pre>
Fixer stats for <em>{command line string}</em>
INFO:root:Fixer time: <em>{time in seconds}</em>
INFO:root:Fixer mem: <em>{memory usage in MB}</em>
</pre>

- If a command is run with the **full heuristic (Full-AA)**, the command line will end with **`-heuristic-raising`**.
- If the command is run with the **trace heuristic (Trace-AA)**, the command line will instead end with **`-heuristic-raising -trace-aa`**

Example for PMDK:

<pre>
Fixer stats for "/usr/lib/llvm-8/bin/opt -load /home/iangneal/workspace/pm-bug-fixing/build/src/PMFIXER.so -pm-bug-fixer -trace-file /home/iangneal/workspace/pm-bug-fixing/build/tests/validation/obj_toid_TEST0_8bbb0af9c/pmemcheck0.trace -fix-summary-file=obj_toid_TEST0_8bbb0af9c_summary.txt <b>-heuristic-raising</b> /tmp/tmp9r8nsj59/obj_toid.static-debug_linked.bc":
INFO:root:Fixer time: 00:00:05 <b>(5.251097 seconds)</b>                                
INFO:root:Fixer mem: <b>86.9296875</b> MB
</pre>




[//]: # (Links below)
