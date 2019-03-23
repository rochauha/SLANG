struct A {
  union {
    int x;
    float y;
  };

  union {
    int z;
    union {
      char c;
      int a;
    };
  };
  int zz;
};

int main() {
  struct A a;
  a.x = 10;
  a.z = 20;
  a.c = 10;
}
