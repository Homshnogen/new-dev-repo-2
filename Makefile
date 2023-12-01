BUILD_PASS_OPTIONS = -fPIC -fno-exceptions -I/usr/lib/llvm-14/include -std=c++17 -Wl,-rpath-link, -Wl,--gc-sections -Wl,-rpath,"/usr/lib/llvm-14/lib" /usr/lib/llvm-14/lib/libLLVM-14.so.1 -shared

all: passes/CSC512Pass.so passes/SkeletonPass.so

passes/CSC512Pass.so:
	clang $(BUILD_PASS_OPTIONS) -o passes/CSC512Pass.so src/CSC512.cpp
	
passes/SkeletonPass.so:
	clang $(BUILD_PASS_OPTIONS) -o passes/SkeletonPass.so src/Skeleton.cpp 

test-cases: all
	clang -fpass-plugin="passes/CSC512Pass.so" test-cases/hello.c -o test-output/hello1
	clang -fpass-plugin="passes/SkeletonPass.so" test-cases/hello.c -o test-output/hello2
	clang -fpass-plugin="passes/CSC512Pass.so" test-cases/something.c -o test-output/something1
	clang -fpass-plugin="passes/SkeletonPass.so" test-cases/something.c -o test-output/something2

clean-test-cases:
	rm -fv test-output/*

clean: clean-test-cases
	rm -fv passes/*