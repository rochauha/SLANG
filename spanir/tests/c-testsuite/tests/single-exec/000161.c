int
main()
{
	int arr[2][4];
	int *p;

	p = &arr[1][3];
	*p = 0;
	return arr[1][3];
}
