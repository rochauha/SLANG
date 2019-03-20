struct inner_t {
	int b1, b2;
	struct inner_t *t;
};

typedef struct {
	int a1;
	struct inner_t a2;
	struct {
		int** x;
		int p;
	} t1;
} my_type;


int main() {
	my_type p;
	p.a1 = 200;
}
