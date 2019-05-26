#include<stdio.h>                                                               
#include<malloc.h>
void add(int a, int b);

int main() {
  int a=10,b=20;                                                                  
  void (*fun_ptr)(int,int)=&add;                                                  
  (*fun_ptr)(a,b);                                                                
}                  

void add(int x, int y) {
  int c;                                                                      
  int *p = (int*)malloc(4);
  c=x+y;                                                                        
  //use(c);                                                                       
  printf("Hello\n");
}                                                                               

