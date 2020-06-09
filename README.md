# Hippocrates (ASPLOS '21)

This repository contains the artifact for our ASPLOS '21 paper on Hippocrates, 
an automated tool for fixing PM durability bugs.

# Artifact Evaluation

The remainder of this document 
describes how to run Hippocrates and reproduce the key results from our paper.

### Requirements

This has been tested on a server configured with Ubuntu 20.04.1 LTS.

Other requirements can be installed via the `./install-deps.sh` script. Python requirements
can be found in `requirements.txt`.

### Resources

**Server login**: For the artifact evaluators, we will be providing access to one of our servers, which has
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

```shell
# Requires root priviledges (sudo)
./install-deps.sh
```

#### [GitHub] Setting up the repository (once)

Use the instructions in this section if you are cloning the repository from
GitHub. If you are using the Zenodo archive, skip to the Zenodo instructions below.

After cloning the repository, you need to set up the submodules, which Hippocrates
depends on for both the LLVM pass and for running tests.

```shell
git clone https://github.com/efeslab/hippocrates.git
cd hippocrates

git submodule init
git submodule update --recursive
```

If you need to re-initialize the submodules for any reason, try:

```shell
git submodule deinit --all -f
git submodule init
git submodule update --recursive
```

#### [Zenodo] Setting up the repository (once)

Only use these instructions if you are using the Zenodo archive.

```shell
unzip <download>
cd efeslab-hippocrates-<revision#>
./zenodo-setup.sh
```

The `zenodo-setup.sh` script clones the requires submodules without using the
`git submodule` commands.

#### Compiling Hippocrates

Starting in the root of the repository:

```shell
source build.env

mkdir build
cd build
cmake ..

make PMFIXER -j$(nproc)
make FLUSHREMOVER -j$(nproc)
make PMTEST
# Must build PMEMCHECK before PMINTRINSICS
make PMEMCHECK
make PMINTRINSICS
```

#### Updating the repository

When updating the repository, to update any of the files generated in the 
`build` directory, you must rerun the compilation steps to rebuild these files.
If any of the dependencies are updated, you must rerun the steps listed in 
"Building Dependencies" section (below).

Note that this also includes updates to scripts in the `tools` directory, as
they are only copied/configured when the above `make` or `cmake` commands are
rerun. Just running `git pull` will not automatically apply changes to any
files in the `build` directory.

### Building Dependencies

- wllvm (**optional**, only if installing on a new machine):

```shell
cd deps/whole-program-llvm
sudo -H pip3 install -e . 
```

- `pmemcheck`:

This should already be built, however if you need to rebuild:

```shell
source build.env
cd build
make -C $REPO_ROOT/deps/valgrind-pmem clean
make PMEMCHECK
```

- PMDK:

```shell
source build.env
cd build
make PMDK

cd $REPO_ROOT/build/deps/pmdk/lib/pmdk_debug
extract-bc libpmem.so
extract-bc libpmemobj.so
```

- memcached-pmem (requires PMDK to be built first)

```shell
source build.env
cd $REPO_ROOT/deps/memcached-pmem
git checkout master
git pull

cd $REPO_ROOT/build
make MEMCACHED_PMEM

cd $REPO_ROOT/build/deps/memcached-pmem/bin/
llvm-link-8 memcached.bc -o memcached.linked.bc $REPO_ROOT/build/deps/pmdk/lib/pmdk_debug/libpmem.so.bc
```

- RECIPE (P-CLHT)

```shell
source build.env
cd build
make p-clht_example

cd ./deps/RECIPE/P-CLHT/
extract-bc p-clht_example
llvm-link-8 p-clht_example.bc --override=../../pmdk/lib/pmdk_debug/libpmem.so.bc \
        --override=../../pmdk/lib/pmdk_debug/libpmemobj.so.bc -o p-clht_example.linked.bc
```

- Redis-pmem:

```shell
# Note: build.env will define the REPO_ROOT variable
source build.env
cd build

make REDIS -j$(nproc)
# do this if it hasn't been done already
make FLUSHREMOVER -j$(nproc)
make PMEMCHECK -j$(nproc)
make PMTEST -j$(nproc)

cd $REPO_ROOT/deps/redis/src
extract-bc redis-server
llvm-link-8 redis-server.bc --override=$REPO_ROOT/build/deps/pmdk/lib/pmdk_debug/libpmem.so.bc \
        --override=$REPO_ROOT/build/deps/pmdk/lib/pmdk_debug/libpmemobj.so.bc -o redis-server.linked.bc
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

```shell
cd build
./verify
```

The output should look like:

```shell
...
11 issues resolved:
[447, 452, 458, 459, 460, 461, 585, 940, 942, 943, 945]
```

If you instead want to run each issue individually, run the following:
```shell
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
```shell
source build.env
cd build

rm /mnt/pmem/pool
./deps/valgrind-pmem/bin/valgrind --tool=pmemcheck --log-file=recipe.log ./deps/RECIPE/P-CLHT/p-clht_example 100 1
./parse-trace pmemcheck recipe.log -o recipe.trace
```

2. Apply Hippocrates to fix the bugs:
```shell
source build.env
cd build

./apply-fixer ./deps/RECIPE/P-CLHT/p-clht_example.linked.bc recipe.trace -o ./deps/RECIPE/P-CLHT/fixed \
        --cxx --extra-opt-args="-fix-summary-file=recipe_summary.txt -heuristic-raising -trace-aa"
```

3. Rerun pmemcheck and generate a new bug report:
```shell
rm -f /mnt/pmem/pool 
./deps/valgrind-pmem/bin/valgrind --tool=pmemcheck --log-file=recipe_fixed.log ./deps/RECIPE/P-CLHT/fixed 100 1
```

4. Confirm the bug report is empty, thus confirming the bugs are fixed:
```shell
./parse-trace pmemcheck recipe_fixed.log -o recipe_fixed.trace
```

This should produce output similar to the following:
```shell
Identified trace start


Identified trace end
...
Prepare to dump.
        Num items: 1
Prepare to write.
Report written to recipe_fixed.trace
```

#### memcached-pmem bugs

This assumes you have run the instructions for building memcached-pmem and pmemcheck, which
you can find above.


After building memcached, the bug-fixing process can be automated as follows:

```shell
source build.env
cd $REPO_ROOT/build

./verify-memcached --help
./verify-memcached 
```

The `verify-memcached` can also run over the prebuilt files present in `artifact/prebuilt`
if you run using the `--use-prebuilt` flag.

For example:

```shell
source build.env
cd $REPO_ROOT/build

./verify-memcached --use-prebuilt
```

To reproduce the memcached-pmem bugs manually, do the following:

1. First, find the bugs and generate a pmemcheck trace:

```shell
source build.env
cd $REPO_ROOT/build/deps/memcached-pmem/bin

LD_LIBRARY_PATH=$(realpath $REPO_ROOT/build/deps/pmdk/lib/pmdk_debug) \
        $REPO_ROOT/build/deps/valgrind-pmem/bin/valgrind --tool=pmemcheck \
        --log-file=memcached.log ./memcached -m 0 -U 0 -t 1 -A -o \
        pslab_file=/mnt/pmem/pool-$(whoami),pslab_size=8,pslab_force
```

In a second terminal:
```shell
source build.env

telnet localhost 11211
# Commands prefixed with ">" are entered inside of telnet
> set foo 1 0 3
> bar
> shutdown
```

Then parse the trace:
```shell
# This can be run in either terminal 1 or 2, but assumes you have run 
# `source build.env` already.
cd $REPO_ROOT/build

./parse-trace pmemcheck ./deps/memcached-pmem/bin/memcached.log -o ./deps/memcached-pmem/bin/memcached.trace
```

2. Then, apply Hippocrates:
```shell
source build.env
cd $REPO_ROOT/build

./apply-fixer ./deps/memcached-pmem/bin/memcached.linked.bc ./deps/memcached-pmem/bin/memcached.trace \
        -o ./deps/memcached-pmem/bin/memcached-fixed --keep-files \
        --extra-opt-args="-fix-summary-file=memcached_summary_time.txt -heuristic-raising -trace-aa" 
```

3. Repeat steps 1, except using the fixed binary. Then confirm the new binary does not have any bugs:

```shell
# In terminal 1
source build.env
cd $REPO_ROOT/build/deps/memcached-pmem/bin

LD_LIBRARY_PATH=$(realpath $REPO_ROOT/build/pmdk/lib/pmdk_debug) $REPO_ROOT/build/deps/valgrind-pmem/bin/valgrind \
        --tool=pmemcheck --log-file=memcached_fixed.log ./memcached-fixed -m 0 -U 0 -t 1 -A -o \
        pslab_file=/mnt/pmem/pool-$(whoami),pslab_size=8,pslab_force
```

```shell
# In terminal 2
source build.env
cd $REPO_ROOT/build

./parse-trace pmemcheck ./deps/memcached-pmem/bin/memcached_fixed.log -o ./deps/memcached-pmem/bin/memcached_fixed.trace
```

The output should be like the following:

```shell
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

```shell
# Start in the root of the repository, e.g. /home/your_username/hippocrates_repo
# --- Note: build.env will define the REPO_ROOT variable
source build.env
cd $REPO_ROOT/build

# This creates the baseline for the redis experiment
./remove-flushes $REPO_ROOT/deps/redis/src/redis-server.linked.bc -o $REPO_ROOT/deps/redis/src/redis-server-noflush -s
```

- The following will be run in two terminals (to collect the trace):

```shell
# TERMINAL 1
# --- Start in the root of the repository, e.g. /home/your_username/hippocrates_repo
source build.env
cd $REPO_ROOT/deps/redis/src/

rm -f /mnt/pmem/redis-$(whoami).pmem 
$REPO_ROOT/build/deps/valgrind-pmem/bin/valgrind --tool=pmemcheck \
        --log-file=redis.log ./redis-server-noflush ../../redis-pmem.conf --pmfile /mnt/pmem/redis-$(whoami).pmem 1gb
# Wait until Redis displays the following:
> ... * The server is now ready to accept connections on port 6380
```

```shell
# TERMINAL 2
# --- Start in the root of the repository, e.g. /home/your_username/hippocrates_repo
source build.env

telnet localhost 6380
> set foo bar
# ... (repeat set commands as much as you want)
> shutdown
```

- This creates a `pmemcheck` log, `$REPO_ROOT/deps/redis/src/redis.log`

- After collecting the trace, run the following in either terminal:

```shell
# --- "source build.env" should have already been run
cd $REPO_ROOT/build

# This creates a trace file from the pmemcheck log, which is what Hippocrates uses to locate where to insert bug fixes
./parse-trace pmemcheck $REPO_ROOT/deps/redis/src/redis.log -o $REPO_ROOT/deps/redis/src/redis.trace

# This creates Redis-H-full
./apply-fixer $REPO_ROOT/deps/redis/src/redis-server-noflush.bc $REPO_ROOT/deps/redis/src/redis.trace \
        -o $REPO_ROOT/deps/redis/src/redis-server-trace --keep-files --cxx \
        --extra-opt-args="-fix-summary-file=redis_summary.txt -heuristic-raising"

# This creates Redis-H-intra
./apply-fixer $REPO_ROOT/deps/redis/src/redis-server-noflush.bc $REPO_ROOT/deps/redis/src/redis.trace \
        -o $REPO_ROOT/deps/redis/src/redis-server-dumb --keep-files --cxx \
        --extra-opt-args="-fix-summary-file=redis_intra_summary.txt -disable-raising -intra-only" 

```

Now, run the performance evaluation:

```shell
# --- "source build.env" should have already been run

cd $REPO_ROOT/build
./run-redis -t 10 --output-file $REPO_ROOT/results/test.csv
# if using prebuilt:
./run-redis -t 10 --output-file $REPO_ROOT/results/test.csv -r $REPO_ROOT/artifact/prebuilt

cd $REPO_ROOT/results
./graph.py test.csv --output test.pdf
```

This should create a graph similar to Figure 4 in the paper, and should also produce textual output similar to the following:

```s
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

Your results may vary, however the trend in the data (i.e., Redis<sub>H-intra</sub> is up to 5-10X worse than either Redis-pmem or Redis<sub>H-full</sub> and Redis<sub>H-full</sub> has around the same or slightly better performance than Redis-pmem) should still hold.

### 3. Hippocrates's overhead

This information is gathered from examining `./build/apply_fixer.log`. This is a log which is appended to for every invocation of the `apply-fixer` script.

The format of entries in this log is the following:

<pre>
Fixer stats for <em>{command line string}</em>
INFO:root:Fixer time: <em>{time in seconds}</em>
INFO:root:Fixer mem: <em>{memory usage in MB}</em>
</pre>

Example for PMDK:

<pre>
Fixer stats for "/usr/lib/llvm-8/bin/opt -load /home/iangneal/workspace/pm-bug-fixing/build/src/PMFIXER.so -pm-bug-fixer -trace-file /home/iangneal/workspace/pm-bug-fixing/build/tests/validation/obj_toid_TEST0_8bbb0af9c/pmemcheck0.trace -fix-summary-file=obj_toid_TEST0_8bbb0af9c_summary.txt <b>-heuristic-raising</b> /tmp/tmp9r8nsj59/obj_toid.static-debug_linked.bc":
INFO:root:Fixer time: 00:00:05 <b>(5.251097 seconds)</b>                                
INFO:root:Fixer mem: <b>86.9296875</b> MB
</pre>



[//]: # (Links below)
