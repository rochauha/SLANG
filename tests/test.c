// #include <stdio.h>

int sum(int a, int b);
// {
// 	return a + b;
// }

int mul(int a, int b);
// {
// 	return a * b;
// }

typedef int INT;

int a = 0, *p;

int main() {
	// int b = 0;
	INT (*fptr)(int, int);

	fptr = &sum;	
	// a = 340;
	 a = fptr(5, 45) + sum(55, 540);
	// printf("%d\n", a);

	// p = &a;

	// p = &a;

	// fptr = &mul;
	// a = fptr(5, 45);
	// printf("%d\n", a);
}
