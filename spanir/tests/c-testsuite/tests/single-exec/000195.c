struct inner_type {
	int b1, b2;
	struct inner_type* next;
};

int main() {
	struct inner_type q = {1, 2, 3};
}
