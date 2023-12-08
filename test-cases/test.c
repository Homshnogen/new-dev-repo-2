#include <stdio.h>

struct abg {
    int xx, yy;
};

struct {
    int aa, bb;
} *j;

struct cya {
    int ss, tt;
} boggy(int c) {
    struct cya temp;
    temp.ss = 9;
    temp.tt = c;
    return temp;
}
typedef struct u {
    int i;
    struct {int b1, b2, b3; union {int b4, b5;};};
    struct {int b1, b2, b3;} foof;
} yyy;

yyy ghjk;


int flor(int a) {
    return (a == 0) ? a : (a + flor(a/2));
};

int carat(int a) {
    ghjk.i = 3;
    ghjk.b1 = 4;
    ghjk.b2 = 5;
    ghjk.b3 = 6;
    ghjk.b4 = 7;


    struct leas {
        int q;
    };
    printf("%d\n", ((struct leas){5}).q);
    printf("%d\n", ((struct {int r4, r5, r6, r7, r8;}){5, 6, a, 8, 9}).r6);

    return boggy(a).tt*2 + 1;
}

void morbs(int a) {
    printf("%d", a);
}

int main() {
    int *it;
    (*j).bb = 6;
    printf("Helo %d'z nuts", carat(41));
    printf("Helo %d'z nuts", carat(carat(41)));

    struct abg k;
    struct gtj{
        int aa, bb;
    } m;


    int cal[8][10];
    it = &cal[5][j->bb];
    scanf("%d", it);
    int (*fptr)(int) = (cal[5][j->bb]%2 == 1) ? &carat : &flor;
    printf("Helo %d'z nuts", (*fptr)(10));
    char *soot = "Helo %d'z nuts";
    char gum = "Helo %d'z nuts"[11];
    gum = soot[12];
    
    return 0;
}