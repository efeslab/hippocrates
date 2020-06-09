PATH2LIB=./LLVMHEURISTIC.so        # Specify your build directory in the project
PASS=-fplicm-correctness                   # Choose either -fplicm-correctness or -fplicm-performance

# Delete outputs from previous run.
rm -f default.profraw ${1}_prof ${1}_fplicm ${1}_no_fplicm *.bc ${1}.profdata *_output *.ll

# Convert source code to bitcode (IR)
clang -emit-llvm -c ${1}.c -o ${1}.bc
# Canonicalize natural loops
opt -loop-simplify ${1}.bc -o ${1}.ls.bc
# Instrument profiler
opt -pgo-instr-gen -instrprof ${1}.ls.bc -o ${1}.ls.prof.bc
# Generate binary executable with profiler embedded
clang -fprofile-instr-generate ${1}.ls.prof.bc -o ${1}_prof

# Generate profiled data
./${1}_prof > correct_output
llvm-profdata merge -o ${1}.profdata default.profraw

# Apply FPLICM
opt -o ${1}.fplicm.bc -pgo-instr-use -pgo-test-profile-file=${1}.profdata -load ${PATH2LIB} ${PASS} < ${1}.ls.bc > /dev/null

