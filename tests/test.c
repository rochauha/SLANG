// MIT License.
// Copyright (c) 2019 The SLANG Authors.

// test1.c: A sample test program. (This file)
// test1.py: SPAN IR module for this file.

int main() {
	int x = 30;
	for (int i = 0, j = 340; i < 5 && x; i = i + 3) {
		x = 11111111111;
	}

	x = 999 + x * 5;
}
