BUILD_PASS_OPTIONS = -fPIC -fno-exceptions -I/usr/lib/llvm-14/include -std=c++17 -Wl,-rpath-link, -Wl,--gc-sections -Wl,-rpath,"/usr/lib/llvm-14/lib" /usr/lib/llvm-14/lib/libLLVM-14.so -shared
DEBUG_FLAGS = -g -fno-discard-value-names -O0

pass: passes/CriticalInputPass.so

passes/CriticalInputPass.so: src/CriticalInput.cpp
	clang $(BUILD_PASS_OPTIONS) -o passes/CriticalInputPass.so src/CriticalInput.cpp

prof-all: prof-something prof-hello prof-mcf

run-all: run-something run-hello run-mcf

clean-test-cases: clean-mcf-output
	rm -fv test-output/*

test-output/mcf: test-cases/mcf.c
	clang test-cases/mcf.c -o test-output/mcf
test-output/hello: test-cases/hello.c
	clang test-cases/hello.c -o test-output/hello
test-output/something: test-cases/something.c
	clang test-cases/something.c -o test-output/something

run-mcf: test-output/mcf clean-mcf-output
	mkdir test-output/mcf-output
	cd test-output/mcf-output; ../mcf ../../test-cases/mcf1.in 1
run-mcf-big: test-output/mcf clean-mcf-output
	mkdir test-output/mcf-output
	cd test-output/mcf-output; ../mcf ../../test-cases/mcf2.in 2

run-something: test-output/something
	./test-output/something
run-hello: test-output/hello
	./test-output/hello
	./test-output/hello test-cases/hello1.in
	./test-output/hello test-cases/hello2.in
	

prof-hello: passes/CriticalInputPass.so test-cases/hello.c
	clang $(DEBUG_FLAGS) -emit-llvm -fpass-plugin="passes/CriticalInputPass.so" -S test-cases/hello.c -o test-output/hello.ll

prof-mcf: passes/CriticalInputPass.so test-cases/mcf.c
	clang $(DEBUG_FLAGS) -emit-llvm -fpass-plugin="passes/CriticalInputPass.so" -S test-cases/mcf.c -o test-output/mcf.ll

prof-something: passes/CriticalInputPass.so test-cases/something.c
	clang $(DEBUG_FLAGS) -emit-llvm -fpass-plugin="passes/CriticalInputPass.so" -S test-cases/something.c -o test-output/something.ll

clean-mcf-output:
	rm -dfv test-output/mcf-output/* test-output/mcf-output

clean: clean-test-cases
	rm -fv passes/*
