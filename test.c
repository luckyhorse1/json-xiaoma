#include "leptjson.h"
#include <stdio.h>

static int main_ret = 0;
static int test_count = 0;
static int test_pass = 0;

#define EXPECT_EQ_BASE(equality, expect, actual, format)\
	do {\
		test_count++;\
		if (equality) {\
			test_pass++;\
		} else{\
			fprintf(stderr, "%s:%d expect: " format ", actual: " format "\n", __FILE__, __LINE__, expect, actual);\
			main_ret = 1;\
		}\
	} while(0)

#define EXPECT_EQ_INT(expect, actual) EXPECT_EQ_BASE((expect) == (actual), expect, actual, "%d")

#define TEST_ERR(err_type, json)\
	do {\
		lept_value v;\
		v.type = LEPT_NULL;\
		EXPECT_EQ_INT(err_type, lept_parse(&v, json));\
		EXPECT_EQ_INT(LEPT_NULL, lept_get_type(&v));\
	} while(0)

static void test_parse_all_blank() {
	TEST_ERR(LEPT_PARSE_ALL_BLANK, "");
	TEST_ERR(LEPT_PARSE_ALL_BLANK, " \t \n \r");
}

static void test_parse_invalid_type() {
	TEST_ERR(LEPT_PARSE_INVALID_TYPE, "nul f");
	TEST_ERR(LEPT_PARSE_INVALID_TYPE, "?");
}

static void test_parse_not_single() {
	TEST_ERR(LEPT_PARSE_NOT_SINGLE, "null f");
	TEST_ERR(LEPT_PARSE_NOT_SINGLE, "true ?");
}

static void test_parse_null() {
	lept_value v;
	v.type = LEPT_NULL;
	EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "null"));
	EXPECT_EQ_INT(LEPT_NULL, lept_get_type(&v));
}

static void test_parse_true() {
	lept_value v;
	v.type = LEPT_NULL;
	EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "true"));
	EXPECT_EQ_INT(LEPT_TRUE, lept_get_type(&v));
}

static void test_parse_false() {
	lept_value v;
	v.type = LEPT_NULL;
	EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "false"));
	EXPECT_EQ_INT(LEPT_FALSE, lept_get_type(&v));
}

static void test_parse() {
	test_parse_all_blank();
	test_parse_invalid_type();
	test_parse_not_single();
	test_parse_null();
	test_parse_true();
	test_parse_false();
}

int main(void) {
	test_parse();
	printf("%d %d (%3.2f%%) passed\n", test_pass, test_count, test_pass*100.0/test_count);
	return main_ret;
}
