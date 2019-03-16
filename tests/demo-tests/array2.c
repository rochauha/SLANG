#include <stdio.h>

int func(int arr[]) {
  printf("%d", arr[0]);
  return arr[0];
}

int main(int argc, char **argv) {
  int x = 20;
  scanf("%d", &x);

  int arr[x][10];
  ++x;
  int arr1[x+2+func(arr[0][0])];
  arr1[0] = 10;
  func(arr1);

  return 0;
}
