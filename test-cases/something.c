#include <stdio.h>

int flor(int a) {
    return (a == 0) ? a : (a + flor(a/2));
};

int carat(int a) {
    return a*2 + 1;
}

int main() {
    printf("Helo %d\n", carat(41));
    printf("Helo %d\n", carat(carat(41)));
    int cal;
    int other;
    printf("Put two numbers\n");
    scanf("%d %d", &cal, &other);
    int (*fptr)(int) = (cal%2 == 1) ? &carat : &flor;
    printf("Helo %d\n", (*fptr)(other));
    
    return 0;
}