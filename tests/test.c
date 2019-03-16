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
INT (*fptr)(int, int);

int main() {
	// int b = 0;
	// fptr = &sum;	
	// a = 340;
	// a = fptr(5, 45);
	// printf("%d\n", a);

	// p = &a;


	a = mul(4, 54) + sum(44, 55);
	p = &a;

	// fptr = &mul;
	// a = fptr(5, 45);
	// printf("%d\n", a);
}
