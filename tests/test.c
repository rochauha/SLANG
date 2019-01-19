int main(int argc, char** argv) {
	int x = 20;
	int *y = &x;
	int *z = &x;

	*z = 200 * 5 + x;

	if (*(y + 300 * x + *z)) {
		x = 30;
	}
}