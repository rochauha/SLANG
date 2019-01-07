// MIT License.
// Copyright (c) 2019 The SLANG Authors.

// test2.c: A sample test program. (This file)
// test2.py: SPAN IR module for this file.

int main() {
  int b,x,y;
  b = 1;
  if (b)
    y = 20;
  else
    y = x;
  return y; // to simulate use(y)
}
