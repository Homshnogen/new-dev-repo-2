#include <stdio.h>

int collatz_count_until_1(unsigned int n) {
  int count = 0;
  while(n != 1) {
    if(n % 2 == 0) {
      n /= 2;
    } else {
      n = (3 * n) + 1;
    }
    count++;
  }
  return count;
}

int main(void) {
  int length = 0;
  int number = 0;
  for(int i = 1; i <= 30; i++) {
    int l = collatz_count_until_1(i);
    if(length < l) {
      length = l;
      number = i;
    }
  }

  printf("Maximum stopping distance %d, starting number %d\n", length, number);
}
