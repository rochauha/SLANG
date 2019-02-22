#include <stdio.h>

#define PRINT printf("%s, %d\n", __FILE__, __LINE__)

int main() {
  PRINT;
  PRINT;
}
