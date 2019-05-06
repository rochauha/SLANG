#include<stdio.h>
int *add(int *c,int *x, int *y)
{  
	//printf("%d,%d",*x,*y);
	*c=*x+*y;
	return c;

}
int main()
{
int *c;
int a=10,b=20;
int *d;
d=add(&c,&a,&b);
//printf("%d\n", *d);
//use(d);
}

