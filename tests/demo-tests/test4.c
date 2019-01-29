
int main() {
  int a, b, c, *u;
  a = 10;
  c = 20;
  b = 2;
  if(b)
    u = &a;
  else
    u = &c;
  // *u = b; // this works too!
  c = *u;
  return c;
}
