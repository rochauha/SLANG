int y = 30;
int main() {
  int x = 10;

label:
  if (x) {
    x = 20;
  } else {
    x = y;
  }

  return x;
}
