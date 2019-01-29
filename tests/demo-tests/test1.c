// MIT License.
// Copyright (c) 2019 The SLANG Authors.

// test1.c: A sample test program. (This file)
// test1.py: SPAN IR module for this file.

int g;
void main(int argc, char **argv) {
  int x = 10, y, z;
  x = 10;
  y = 20;
  z = y;
  g = z;
}
