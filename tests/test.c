// MIT License.
// Copyright (c) 2019 The SLANG Authors.

// test1.c: A sample test program. (This file)
// test1.py: SPAN IR module for this file.

int g;

int main(int argc, char **argv) {
    int x, y, z;
    x = 10;
    y = x;
    z = y;
    g = z;
}
