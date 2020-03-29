#include <stdio.h>
#include <stdlib.h>

#include <ilm.h>

int main(void)
{
	int ret, s;

	ret = ilm_connect(&s);
	if (ret == 0) {
		printf("ilm_connect: SUCCESS\n");
	} else {
		printf("ilm_connect: FAIL\n");
		exit(-1);
	}

	ret = ilm_disconnect(s);
	if (ret == 0) {
		printf("ilm_disconnect: SUCCESS\n");
	} else {
		printf("ilm_disconnect: FAIL\n");
		exit(-1);
	}

	return 0;
}
