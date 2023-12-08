BUILD_PASS_OPTIONS = -fPIC -fno-exceptions -I/usr/lib/llvm-14/include -std=c++17 -Wl,-rpath-link, -Wl,--gc-sections -Wl,-rpath,"/usr/lib/llvm-14/lib" /usr/lib/llvm-14/lib/libLLVM-14.so -shared
DEBUG_FLAGS = -g -fno-discard-value-names -O0

all: passes/CriticalInputPass.so passes/PracticePass.so passes/BranchPointerProfilerPass.so

passes/CriticalInputPass.so: src/CriticalInput.cpp
	clang $(BUILD_PASS_OPTIONS) -o passes/CriticalInputPass.so src/CriticalInput.cpp
	
passes/PracticePass.so: src/Practice.cpp 
	clang $(BUILD_PASS_OPTIONS) -o passes/PracticePass.so src/Practice.cpp 

passes/BranchPointerProfilerPass.so: src/BranchPointerProfiler.cpp 
	clang $(BUILD_PASS_OPTIONS) -o passes/BranchPointerProfilerPass.so src/BranchPointerProfiler.cpp 

test-cases: all test-output/hello.ll test-output/something.ll test-output/mcf.ll test-output/test.ll

test-output/hello.ll: passes/BranchPointerProfilerPass.so test-cases/hello.c
	clang $(DEBUG_FLAGS) -emit-llvm -fpass-plugin="passes/BranchPointerProfilerPass.so" -S test-cases/hello.c -o test-output/hello.ll
	@# clang test-output/hello.ll -o test-output/hello

test-output/something.ll: passes/BranchPointerProfilerPass.so test-cases/something.c
	clang $(DEBUG_FLAGS) -emit-llvm -fpass-plugin="passes/BranchPointerProfilerPass.so" -S test-cases/something.c -o test-output/something.ll
	@# clang test-output/something.ll -o test-output/something

test-output/mcf.ll: passes/BranchPointerProfilerPass.so test-cases/mcf.c
	clang $(DEBUG_FLAGS) -emit-llvm -fpass-plugin="passes/BranchPointerProfilerPass.so" -S test-cases/mcf.c -o test-output/mcf.ll
	@# clang test-output/mcf.ll -o test-output/mcf

test-output/test.ll: passes/BranchPointerProfilerPass.so test-cases/test.c
	clang $(DEBUG_FLAGS) -emit-llvm -fpass-plugin="passes/BranchPointerProfilerPass.so" -S test-cases/test.c -o test-output/test.ll
	@# clang test-output/test.ll -o test-output/test

clean-test-cases: clean-mcf-output
	rm -fv test-output/*

test-output/mcf:
	clang test-cases/mcf.c -o test-output/mcf

run-mcf: test-output/mcf clean-mcf-output
	mkdir test-output/mcf-output
	@cd test-output/mcf-output; ../mcf ../../test-cases/mcf.in

test-practice: passes/PracticePass.so test-cases/test.c
	clang $(DEBUG_FLAGS) -emit-llvm -fpass-plugin="passes/PracticePass.so" -S test-cases/test.c -o test-output/test.ll

test-mcf: passes/PracticePass.so test-cases/mcf.c
	clang $(DEBUG_FLAGS) -emit-llvm -fpass-plugin="passes/PracticePass.so" -S test-cases/mcf.c -o test-output/mcf.ll

test-ci: passes/CriticalInputPass.so test-cases/test.c
	clang $(DEBUG_FLAGS) -emit-llvm -fpass-plugin="passes/CriticalInputPass.so" -S test-cases/test.c -o test-output/test.ll

clean-mcf-output:
	rm -dfv test-output/mcf-output/* test-output/mcf-output

clean: clean-test-cases
	rm -fv passes/*