PROGS = full_copy
LIBS = -lpmem

full_copy: 
	clang -emit-llvm full_copy.c $(LIBS)

clean:
	rm -v $(PROGS)
