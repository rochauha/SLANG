// MIT License.
// Copyright (c) 2019 The SLANG Authors.

// test2.c: A sample test program. (This file)
// test2.py: SPAN IR module for this file.

int main(int argc) {
  int b,x,y;
  b = argc - 10;
  if (b)
    y = 20;
  else
    y = x;
  return y; // to simulate use(y)
}
