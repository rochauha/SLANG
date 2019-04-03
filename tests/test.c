// MIT License.
// Copyright (c) 2019 The SLANG Authors.

// test1.c: A sample test program. (This file)
// test1.py: SPAN IR module for this file.


enum my_type {ANSHUMAN, RONAK, RESHABH};

int main() {
	int x = 400, y = 3400, z = 0;
	
	if (x) {
		y = 400;
	}

	while (x > 54) {
		x = 300 + x;
	}
	z = x + y;
}
