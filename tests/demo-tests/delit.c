int main(int argc) {
  int x, y;
  int *p;
  x = 5;
  y = 6;

  p = &x;
  *p = 20;

  return y;
}
