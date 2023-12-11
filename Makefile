BUILD_PASS_OPTIONS = -fPIC -fno-exceptions -I/usr/lib/llvm-14/include -std=c++17 -Wl,-rpath-link, -Wl,--gc-sections -Wl,-rpath,"/usr/lib/llvm-14/lib" /usr/lib/llvm-14/lib/libLLVM-14.so -shared
DEBUG_FLAGS = -g -fno-discard-value-names -O0
SRC_DIR = src
PASSES_DIR = passes
TEST_CASES_DIR = test-cases
TEST_OUTPUT_DIR = test-output
PROFILING_DIR = profiling

SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
PASS_SO = $(patsubst $(SRC_DIR)/%.cpp,$(PASSES_DIR)/%.so,$(SOURCES))
TEST_CASES = $(wildcard $(TEST_CASES_DIR)/*.c)
TEST_CASES_LL = $(patsubst $(TEST_CASES_DIR)/%.c,$(TEST_OUTPUT_DIR)/%.ll,$(TEST_CASES))
TEST_CASES_EXE = $(patsubst $(TEST_CASES_DIR)/%.c,$(TEST_OUTPUT_DIR)/%,$(TEST_CASES))

# Default LLVM pass to be applied
DEFAULT_PASS = BranchPointerProfiler

all: $(PASS_SO) $(TEST_CASES_LL) $(TEST_CASES_EXE)

$(PASSES_DIR)/%.so: $(SRC_DIR)/%.cpp
	clang $(BUILD_PASS_OPTIONS) -o $@ $<

$(TEST_OUTPUT_DIR)/%.ll: $(TEST_CASES_DIR)/%.c $(PASSES_DIR)/$(DEFAULT_PASS).so
	clang $(DEBUG_FLAGS) -emit-llvm -fpass-plugin="$(PASSES_DIR)/$(DEFAULT_PASS).so" -S $< -o $@

$(TEST_OUTPUT_DIR)/%: $(TEST_OUTPUT_DIR)/%.ll
	clang $< -o $@

PROFILING_DIR = profiling
VALGRIND_FLAGS = --tool=callgrind

# Profiling with Valgrind
profile: $(TEST_CASES_EXE)
	mkdir -p $(PROFILING_DIR)
	$(foreach exe,$(TEST_CASES_EXE),valgrind $(VALGRIND_FLAGS) --callgrind-out-file=$(PROFILING_DIR)/$(notdir $(exe)).callgrind ./$(exe);)

# Analyze profiling data
analyze:
	./tools/analyze_profiling_data.sh $(PROFILING_DIR)


clean:
	rm -f $(PASSES_DIR)/*.so
	rm -f $(TEST_OUTPUT_DIR)/*.ll
	rm -f $(TEST_OUTPUT_DIR)/*
	rm -rf $(PROFILING_DIR)
	rm -f branch_dictionary.txt
