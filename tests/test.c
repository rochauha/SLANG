enum my_type {ANSHUMAN = 1, RONAK, RESHABH = 2};
enum my_type g = ANSHUMAN;
int x = 10, y = 20, *z = &x;
int main() {

	if(x++)
		y++;
	else 
		x++;

	while (y)
		(*z)++;

}
