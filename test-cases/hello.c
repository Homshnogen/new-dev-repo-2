#include <stdio.h>

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
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j <= i; j++) {
            printf("(%d %d %d) ", i, j, what(i, j));
        }
    }
    return 0;
}