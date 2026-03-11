extern void print(int); 

int func(int test) { // function cannot be called "main"
	int loc1;
	int loc2;
	int i;
	int val;
	int a;

	loc1 = a + 10;
	loc2 = a * 10;

	if (loc1 > loc2) { print(loc1); }
	else print(loc2);

	i = 0;
	while (i < loc1) {
		int j;
		j = i + val;
	}

	return (loc1 + loc2);
}
