int
main()
{
	int arr[2][4];
	int *p;
  int x;

  x = 0;

	p = &arr[x+x][x+2+1];
	*p = 0;
	return arr[1][3];
}
