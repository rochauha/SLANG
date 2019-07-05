int main();

int foo() {
  main();
  foo();
  foo();
}

int main() {
  foo();
}

int bar() {
}
