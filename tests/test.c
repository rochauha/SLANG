void *dummy_malloc(unsigned size);

int main() {
	int *x = (int) dummy_malloc(5);
	x = x + 5;
}
