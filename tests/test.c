struct inner_t {
	int b1, b2;
};
typedef struct {
	int a1;
	struct inner_t a2;
} my_type;


int main() {
	// struct {int attr} x;
	// x.attr = 230;
	my_type p;
	p.a1 = 200;
	// p.a2.b1 = 30;
	// p.a2.b2 = 40;
	// // p.a1 = 200;
}
