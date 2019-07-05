#include <stddef.h>
#include <stdio.h>

int main() {
  int x, y;
  x = 0;
  scanf("%d", &x);
  int *p;

  if (x%2 + 1) {
    p = &x;
  } else {
    p = NULL;
  }

  return *p;
}
