#include <stdio.h>

#define G_STRINGIFY(macro_or_string) G_STRINGIFY_ARG(macro_or_string)
#define G_STRINGIFY_ARG(contents) #contents

int main(int argc, char *argv[])
{
	void *p;
	unsigned int x;

	x = 9;

	p = (void*)x;
	printf("%s %s", __FILE__ ":" G_STRINGIFY(__LINE__) ":", __PRETTY_FUNCTION__);
	uintptr_t u = p;

	printf("%ud\n", u);
	return 0;
}
