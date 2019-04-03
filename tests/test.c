// MIT License.
// Copyright (c) 2019 The SLANG Authors.

// test1.c: A sample test program. (This file)
// test1.py: SPAN IR module for this file.


int main() {
	int x, y = 300, z = 0;	
	do {
		x = 300;
		x = x + y;
		z = z - x;
	} while (z > 0);
}
