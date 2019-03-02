struct my_type_t {
	int attr_1;
	int attr_2;
};

int main() {
	const struct my_type_t x = {1, 23};
	struct my_type_t x2 = {2, 42};
	// struct my_type_t *px = &x;
	// struct my_type_t x = {1, 2}, y = {4, 5}, c;
	// c = x;
	x2 = x;
	// enum my_enum_t z = RONAK_C;
}