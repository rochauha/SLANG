
#include <stdio.h>
int main(int argc, char **argv) {
  int i = 10;

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
      print("Hello default\n");
  }

  return 0;
}
