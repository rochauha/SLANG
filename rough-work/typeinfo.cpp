#include <iostream>
#include <typeinfo>

class A {
  public:
    virtual int func() { return 0;}
};

class B : public A { public: int func() { return 1;}};
class C : public A {};

int main() {
  A a;
  B b;
  C c;
  std::cout << int(typeid(&C::func) == typeid(&A::func)) << std::endl;
  std::cout << int(typeid(&B::func) == typeid(&A::func)) << std::endl;
}
