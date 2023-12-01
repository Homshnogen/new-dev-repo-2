BUILD_PASS_OPTIONS = -fPIC -fno-exceptions -I/usr/lib/llvm-14/include -std=c++17 -Wl,-rpath-link, -Wl,--gc-sections -Wl,-rpath,"/usr/lib/llvm-14/lib" /usr/lib/llvm-14/lib/libLLVM-14.so -shared

all: passes/CSC512Pass.so passes/SkeletonPass.so passes/ContainerPass.so passes/BranchPointerProfilerPass.so

passes/CSC512Pass.so:
	clang $(BUILD_PASS_OPTIONS) -o passes/CSC512Pass.so src/CSC512.cpp
	
passes/SkeletonPass.so:
	clang $(BUILD_PASS_OPTIONS) -o passes/SkeletonPass.so src/Skeleton.cpp 

passes/ContainerPass.so:
	clang $(BUILD_PASS_OPTIONS) -o passes/ContainerPass.so src/Container.cpp 

passes/BranchPointerProfilerPass.so:
	clang $(BUILD_PASS_OPTIONS) -o passes/BranchPointerProfilerPass.so src/BranchPointerProfiler.cpp 

test-cases: all
	clang -fpass-plugin="passes/BranchPointerProfilerPass.so" -emit-llvm -c test-cases/hello.c -o test-output/hello.bc
	llvm-dis test-output/hello.bc -o test-output/hello.ll
	clang -fpass-plugin="passes/BranchPointerProfilerPass.so" -emit-llvm -c test-cases/something.c -o test-output/something.bc
	llvm-dis test-output/something.bc -o test-output/something.ll

clean-test-cases:
	rm -fv test-output/*

clean: clean-test-cases
	rm -fv passes/*