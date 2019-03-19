// slang test case

int sum(int a , int b) {
  return a + b;
}

int main() {
  int (*func)(int, int);
  int x;
  func = sum;
  func = func - 1;

  sum(17,13);

  return (func+1)(10,12);
}
