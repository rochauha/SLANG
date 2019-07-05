#include <stdio.h>

int add(int a, float f) {
  printf("%d, %f\n", a, f);
}

int main(int argc, char **argv) {
  int (*KKKKK)(int, float);

  KKKKK = 0;

  KKKKK = add;
  KKKKK(10, 20.5);

  return 0;
}
