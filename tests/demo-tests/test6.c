int g;
int main(int argc, char **argv) {
  int x, y = 1 + 2;
  int *u;
  if (y) {
    u = &x;
  } else {
    *u = y + 1 + 3;
  }
  g = 0;
  return x = func(1);
  g = 1;
}

int func(int x) {
  return x;
}
