void main() {
  int x = 5, y = 6, *z;
  z = &x;
  x = *z = y = 10;
  {
    x = y = 30;
  }
  
  y = x + x++;
  
  x = y + ++y + y;
}

