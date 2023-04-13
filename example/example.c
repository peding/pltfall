#include <stdio.h>
#include <string.h>


int main(int argc, char *argv[])
{
	char buf1[3] = "A\0B";
	char buf2[sizeof(buf1)] = "A\0\0";

	/*printf("buf1 = ");
	for (int i = 0; i < sizeof(buf1); i++) {
		printf("%02x ", (unsigned char)buf1[i]);
	}
	printf("\n");

	printf("buf2 = ");
	for (int i = 0; i < sizeof(buf1); i++) {
		printf("%02x ", (unsigned char)buf2[i]);
	}
	printf("\n");*/

	char *func_ret[] = {"memcmp", "strncmp"};
	int ret = 0;

	if (argc > 1) {
		ret = strncmp(buf1, buf2, sizeof(buf1));
		printf("return value of strncmp(buf1, buf2, %ld): %02x (actual function called: %s)\n", sizeof(buf1), ret, func_ret[ret == 0]);
	}

	ret = memcmp(buf1, buf2, sizeof(buf1));
	printf("return value of memcmp(buf1, buf2, %ld):  %02x (actual function called: %s)\n", sizeof(buf1), ret, func_ret[ret == 0]);

	ret = strncmp(buf1, buf2, sizeof(buf1));
	printf("return value of strncmp(buf1, buf2, %ld): %02x (actual function called: %s)\n", sizeof(buf1), ret, func_ret[ret == 0]);

	ret = memcmp(buf1, buf2, sizeof(buf1));
	printf("return value of memcmp(buf1, buf2, %ld):  %02x (actual function called: %s)\n", sizeof(buf1), ret, func_ret[ret == 0]);

	if (argc <= 1) {
		ret = strncmp(buf1, buf2, sizeof(buf1));
		printf("return value of strncmp(buf1, buf2, %ld): %02x (actual function called: %s)\n", sizeof(buf1), ret, func_ret[ret == 0]);
	}

	return 0;
}
