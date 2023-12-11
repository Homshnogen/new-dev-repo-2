#include <stdio.h>

void thing(int a, int b) {
    for (int i = 0; i < a; i++) {
        printf("Hello World! %d tell\n", b);
    }
}

void unthing(int a, int b) {
    printf("Hello World! %d cnt*tell\n", a*b);
}

int main(int argc, const char* argv[]) {
    printf("Hello World!\n");
    int cnt, tell;
    FILE *file;
    if (argc == 2) {
        fopen(argv[1], "r");
    }
    char c;
    void (*fptr)(int, int);
    scanf("%d %d", &tell, &cnt);

    fptr = !feof(file) ? &thing : &unthing;
    
    (*fptr)(cnt, tell);
    while ((c = getc(file))) {
        if (c > 'a' && c < 'g') {
            printf("%c", c);
        }
    }
    fclose(file);
    return 0;
}