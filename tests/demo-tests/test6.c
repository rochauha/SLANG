void main() {
  int x, *z, y;
  z = &x;
  x = *z = y = 10;
  y = x + x++;
  x = y + ++y + y;
}

