#include <stdio.h>

void thing(int a, int b) {
    for (int i = 0; i < a; i++) {
        printf("Hello World! %d tell\n", b);
    }
}

void unthing(int a, int b) {
    printf("Hello World! %d cnt*tell\n", a*b);
    thing(a, b);
}

int main(int argc, const char* argv[]) {
    printf("put two numbers\n");
    int cnt, tell;
    scanf("%d %d", &tell, &cnt);

    unthing(cnt, tell);
    FILE *file;
    if (argc == 2) {
        file = fopen(argv[1], "r");
        char c;
        while (!feof(file)) {
            c = getc(file);
            if (c > 'a' && c < 'g') {
                printf("%c", c);
            }
        }
        fclose(file);
        printf("\n");
    }

    return 0;
}