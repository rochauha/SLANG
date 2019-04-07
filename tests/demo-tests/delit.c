#include <stdio.h>

int main() {
  int x[4] = {1,2,3,4};
  int *p = x;
  printf("%d\n", p[1]);

  int a[2][2] = {{1,2},{3,4}};
  int (*pp)[2][2] = a;
  printf("%d\n", pp[0][0][0]);

  return 0;
}
