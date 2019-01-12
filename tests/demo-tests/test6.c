int main() {
  int x, y = 1;
  int *u;
  u = &x;

  *u = y + 1 + 3;
  return x;
}

int func(int x) {
  return x;
}
