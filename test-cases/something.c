#include <stdio.h>

int flor(int a) {
    return (a == 0) ? a : (a + flor(a/2));
};

int carat(int a) {
    return a*2 + 1;
}

int main() {
    printf("Helo %d'z nuts", carat(41));
    printf("Helo %d'z nuts", carat(carat(41)));
    int cal;
    scanf("%d", &cal);
    int (*fptr)(int) = (cal%2 == 1) ? &carat : &flor;
    printf("Helo %d'z nuts", (*fptr)(10));
    
    return 0;
}