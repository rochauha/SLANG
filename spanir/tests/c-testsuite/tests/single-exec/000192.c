struct A;

struct B {
  struct A *a;
};

struct A {
  struct B *b;
};

int main() {
	struct A a;
}
