void main() {
  int a, *u, cond, tmp, b;
  a = 11;
  u = &a;
  // input(cond); // special instruction
  while(cond) { // `cond` value is undeterministic.
    tmp = *u; // point-of-interest
    b = tmp % 2;
    if(b) {
      b = 15;
      u = &b;
    } else {
      b = 16;
    }
  }
  return;
}
