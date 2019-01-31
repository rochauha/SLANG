void main() {
  int **z, a, *u, cond, tmp, b;
  a = 11;
  u = &a;
  // input(cond); // special instruction
  while(cond) { // `cond` value is undeterministic.
    tmp = *u; // point-of-interest
    *u = tmp * 2;
    if(b) {
      b = 15;
      *z = &b;
    } else {
      b = 16;
    }
  }
  return;
}
