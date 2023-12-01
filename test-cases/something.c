#include <stdio.h>

void flor(int a) {};

int carat(int a) {
    return a*2 + 1;
}

int main() {
    printf("Helo %d'z nuts", carat(41));
    printf("Helo %d'z nuts", carat(carat(41)));
    return 0;
}