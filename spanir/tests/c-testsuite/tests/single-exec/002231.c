int main() {
  int x = 0;

  x = x++ ? x++ ? 1 : 2 : 2;

  return x;
}
