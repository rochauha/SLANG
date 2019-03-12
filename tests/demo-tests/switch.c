
#include <stdio.h>

float printMe(const char* str) {
  printf(str);
  return 2;
}

int main(int argc, char **argv) {
//int main() {
  int i = 10;
  float f;
  printf("Hello, World %d %d\n",  i , 10);

  f = printMe("Hello\n");
  f = 2.3456;

  i = 22 + 33;
  switch(i) {
    case 10:
      i = 20;
      break;
    default:
      i = i + 21;
    case 20:
      i = i * 3;
    case 30:
      i = i;
      break;
  }

  switch(i) {
    default:
      printf("Hello default\n");
  }

  return 0;
}
