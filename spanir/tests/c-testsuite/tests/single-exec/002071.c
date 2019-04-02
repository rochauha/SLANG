int main() {
  int x = 4, y;
  x = x && (y || 0);

  // if (x && y || x) {
  //   x = 111;
  // }
  return x;
}
