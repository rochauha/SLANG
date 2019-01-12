int main(int argc, char **argv) {
  int x, y = 1;
  int *u;
  u = &x;

  *u = y + 1 + 3;
  return x;
}

int func(int x) {
  return x;
}
