
int main(int argc) {
  for (int i=0; i < 10; i++) {
    if (i > argc) {
      return 0;
    }
  }
  return 1;
}
