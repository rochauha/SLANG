struct inner_type {
	int b1, b2;
	struct inner_type* next;
};	

struct my_type {
	int attr_1;
	struct inner_type attr_2;
};

int main() {
	struct inner_type q = {1, 2, 3};
	struct my_type p = {2, q};
}
