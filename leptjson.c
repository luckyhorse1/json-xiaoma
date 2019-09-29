#include "leptjson.h"
#include <assert.h>
#include <stdlib.h> /* NULL, strtod() */
#include <errno.h> /* errno, ERANGE */
#include <math.h> /* HUGE_VAL */

#define EXPECT(c, ch) do { assert(*c->json == (ch) ); c->json++; } while(0) 
#define ISDIGIT(ch) (ch >= '0' && ch <= '9')
#define ISDIGIT1TO9(ch) (ch >= '1' && ch <= '9')

typedef struct {
	const char * json;
} lept_context;

static void lept_parse_whitespace(lept_context * c){
	const char * p = c->json;
	while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
		p++;
	c->json = p;
}

static int lept_parse_literal(lept_value * v, lept_context * c, const char * literal, lept_type type) {
	size_t i;
	EXPECT(c, literal[0]);
	for(i=0; literal[i+1]; i++) {
		if (c->json[i] != literal[i+1])
			return LEPT_PARSE_INVALID_TYPE;
	}
	c->json += i;
	v->type = type;
	return LEPT_PARSE_OK;
}

static int lept_parse_number(lept_value * v, lept_context * c) {
	const char * p = c->json;
	if (*p == '-') p++;
	if (*p == '0') p++;
	else {
		if (!ISDIGIT1TO9(*p)) return LEPT_PARSE_INVALID_TYPE;
		for (p++; ISDIGIT(*p); p++);
	}
	if (*p == '.') {
		p++;
		if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_TYPE;
		for (p++; ISDIGIT(*p); p++);
	}
	if (*p == 'e' || *p == 'E') {
		p++;
		if (*p == '+' || *p == '-') p++;
		if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_TYPE;
		for (p++; ISDIGIT(*p); p++);
	}
	errno = 0;
	v->n = strtod(c->json, NULL);
	if (errno == ERANGE && (v->n == HUGE_VAL || v->n == -HUGE_VAL))
		return LEPT_PARSE_NUMBER_TOO_BIG;
	c->json = p;
	v->type = LEPT_NUMBER;
	return LEPT_PARSE_OK;
}

static int lept_parse_value(lept_value * v, lept_context * c) {
	switch (*c->json) {
		case 'n':
			return lept_parse_literal(v, c, "null", LEPT_NULL);
		case 't':
			return lept_parse_literal(v, c, "true", LEPT_TRUE);
		case 'f':
			return lept_parse_literal(v, c, "false", LEPT_FALSE);
		case '\0':
			return LEPT_PARSE_ALL_BLANK;
		default:
			return lept_parse_number(v, c);	
	}
}

int lept_parse(lept_value * v, const char * json) {
	lept_context c;
	int ret;
	assert(v != NULL);
	c.json = json;
	v->type = LEPT_NULL;
	lept_parse_whitespace(&c);
	if ((ret = lept_parse_value(v, &c)) == LEPT_PARSE_OK) {
		lept_parse_whitespace(&c);
		if (*c.json != '\0') {
			v->type = LEPT_NULL;
			ret = LEPT_PARSE_NOT_SINGLE;
		}
	}
	return ret;
}

lept_type lept_get_type(const lept_value * v) {
	assert(v != NULL);
	return v->type;
}

double lept_get_number(const lept_value * v) {
	assert( v!=NULL && v->type == LEPT_NUMBER);
	return v->n;
}
