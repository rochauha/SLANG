#include <stdio.h>

#define NBSP1  " "
#define NBSP2  NBSP1 NBSP1
#define NBSP4  NBSP2 NBSP2
#define NBSP6  NBSP2 NBSP8
#define NBSP8  NBSP4 NBSP4
#define NBSP10 NBSP2 NBSP8

int main() {
  printf("%s: hello", NBSP10);
  return 0;
}

