#include <stdio.h>

typedef int (*func_ptr_t)(int, int); // Define a type for function pointers

// Function to log function pointer values
void logFunctionPtrValue(const char* func_name, func_ptr_t func_ptr) {
    printf("*%s_%p\n", func_name, (void*)func_ptr);
}

// Function to log branch decisions
void logBranchDecision(const char* branch_id) {
    printf("%s\n", branch_id);
}

int what(int a, int b) {
    int c = a^b;
    int d = a&b;
    for (int i = 0; i < 10 && c > d; i++) {
        c = a^d;
        d = c&b;
    }
    return c+d;
}

int main() {
    printf("Hello World!\n");
    int cnt;
    scanf("%d", &cnt);

    // Log the value of the function pointer in main
    logFunctionPtrValue("main", (func_ptr_t)main);

    for (int i = 0; i < cnt; i++) {
        for (int j = 0; j <= i; j++) {
            // Log the values of function pointers in the inner loop
            logFunctionPtrValue("what", (func_ptr_t)what);

            // Log the branch decision based on the condition
            if (i < 10 && (i ^ j) > (i & j)) {
                logBranchDecision("br_2");
            } else {
                logBranchDecision("br_3");
            }

            printf("(%d %d %d) ", i, j, what(i, j));
        }
    }
    return 0;
}

