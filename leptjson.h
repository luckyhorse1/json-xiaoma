#ifndef LEPTJSON_H__
#define LEPTJSON_H__
typedef enum {LEPT_NULL, LEPT_TRUE, LEPT_FALSE, LEPT_NUMBER, LEPT_STRING, LEPT_ARRAY, LEPT_OBJECT} lept_type;

typedef struct {
	double n;
	lept_type type;
} lept_value;

enum {
	LEPT_PARSE_OK, 
	LEPT_PARSE_ALL_BLANK, 
	LEPT_PARSE_INVALID_TYPE, 
	LEPT_PARSE_NOT_SINGLE,
	LEPT_PARSE_NUMBER_TOO_BIG
};

int lept_parse(lept_value * v, const char * json);

lept_type lept_get_type(const lept_value * v);

double lept_get_number(const lept_value * v);

#endif /* LEPTJSON_H__ */
