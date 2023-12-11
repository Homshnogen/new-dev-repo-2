#include <stdio.h>


int flor(int a) {
    int ret = 1;
    for (int i = 0; i < a; i++) {
        ret++;
    }
    return ret;
};

int carat(int a) {
    return 1;
}

int main() {
    int *it;
    printf("Helo %d'z nuts", carat(41));
    printf("Helo %d'z nuts", carat(carat(41)));

    int cal;
    it = &cal;
    scanf("%d", it);
    int (*fptr)(int);
    if (cal > 40) {
        fptr = &carat;
    } else {
        fptr = &flor;
    }
    //int (*fptr)(int) = cal ? &carat : &flor;
    printf("Helo %d'z nuts", (*fptr)(10));
    char *soot = "Helo %d'z nuts";
    char gum = "Helo %d'z nuts"[11];
    gum = soot[12];
    
    return 0;
}