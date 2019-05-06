 #include<stdio.h>                                                               
 void add(int x, int y)                                                          
 {   int c;                                                                      
   c=x+y;                                                                        
   //use(c);                                                                       
   printf("Hello\n");
 }                                                                               

 int main()                                                                      
 {                                                                               
 int a=10,b=20;                                                                  
 void (*fun_ptr)(int,int)=&add;                                                  
 (*fun_ptr)(a,b);                                                                
 }                  
