struct inner_type {
	int b1, b2;
	struct inner_type* next;
};	

struct my_type {
	int attr_1;
	struct inner_type attr_2;
};

int main() {
	struct my_type p = {2, {5 + 4, 6, 0}};
}
