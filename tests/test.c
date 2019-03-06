enum my_type {ANSHUMAN = 1, RONAK, RESHABH = 2};
enum my_type g = ANSHUMAN;

int main() {
	enum my_type x = g;
	x = RESHABH * 5 * ANSHUMAN;
}
