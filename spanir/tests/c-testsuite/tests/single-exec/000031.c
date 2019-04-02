int y = 30;
int main() {
label:
  int x = 10;

  if (x) {
    x = 20;
  } else {
    x = y;
  }

  return x;
}
