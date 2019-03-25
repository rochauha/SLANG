int main() {
  int x = 0;

  x = x ? 1 + 2 + x++ : 2;

  return x;
}
