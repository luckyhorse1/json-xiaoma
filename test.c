#include <stdio.h>
#include "leptjson.h"

void test_parse() {
	lept_value v;
	v.type = LEPT_FALSE;
	int res = lept_parse(&v, "null");
	printf("%d\n", res);
}

int main(void) {
	test_parse();
	return 0;
}
