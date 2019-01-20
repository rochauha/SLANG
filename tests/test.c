int g = 0;
int main(int argc, char** argv) {
	int x = 20;
	int *y = &x;
	int *z = &(*(&x));

	*y = *(y + 6) + *(y + 7);
	 
	*z = 200 * 5 + x;
	 
	*(z+2) = *y + x;

	if (*(y + 300 * x + *z)) { 
		x = 30;
		
		int a, b, c;
		a = 30 + x;
		b = a * 30 - b * 5 + a * g;

		if (b) {
			b = ++a;
			a++; // does'nt show any output
		}
	}
}