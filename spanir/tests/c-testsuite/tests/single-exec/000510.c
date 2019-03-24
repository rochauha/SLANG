int x = 0;

int
main()
{
	switch(x)
		case 111:
			;
	switch(x)
		case 112:
			switch(x) {
				case 113:
					goto next;
				default:
					return 11;
			}
	return 22;
	next:
	switch(x)
		case 111:
			return 33;
	switch(x) {
		{
			x = 1 + 1;
			foo:
			case 222:
				return 44;
		}
	}
	switch(x) {
		case 333:
			return x;
		case 444:
			return 55;
		default:
			return 66;
	}
}
