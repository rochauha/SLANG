#include <stdio.h>
int cond;
int foo(int *p);

int main() {
  int a, *u, tmp, b;
  a = 11;
  b = 13;
  cond = 10;
  u = &a;

  b = foo(u);
  printf("%d", b);

  return b;
}

int foo (int *p) {
  return *p;
}
