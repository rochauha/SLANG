struct my_type_t {
	int attr_1;
	int attr_2;
};


int main() {
	struct my_type_t x = {1, 23};
	struct my_type_t *px = &x;
	// struct my_type_t x = {1, 2}, y = {4, 5}, c;
	// c = x;
	x.attr_1 = 4;
	// enum my_enum_t z = RONAK_C;
}