#include <stdio.h>

//void logBranchDecision(int a) {};

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
    for (int i = 0; i < cnt; i++) {
        for (int j = 0; j <= i; j++) {
            printf("(%d %d %d) ", i, j, what(i, j));
        }
    }
    return 0;
}