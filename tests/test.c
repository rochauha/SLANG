int g = 20, x, y, z;
int sum(int a, int b);
int prod(int a, int b, int c);

int main(int argc, char** argv) {
	// prod(1, 2*prod(x, y, 30), 50);
	g = sum(sum(3, 4), g+5) + sum(1, 2);
}