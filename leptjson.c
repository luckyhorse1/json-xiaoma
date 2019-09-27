#include "leptjson.h"
#include <stdio.h>

static int is_blank(char c) {
	if (c == ' ' || c == '\t' || c == '\n') return 1;
	return 0;
}

static int is_not_single(const char * json) {
	const char * p = json;
	while (*p != '\0') {
		if (is_blank(*p)) {
			p++;
			continue;
		} else {
			return 1;
		}
	}
	return 0;
}

static int parse(lept_value * v, const char * json) {
	int type = 0;
	if (json[0] == 'n' && json[1] == 'u' && json[2] == 'l' && json[3] == 'l') {
		type = 1;
		json += 4;
	}
	if (json[0] == 't' && json[1] == 'r' && json[2] == 'u' && json[3] == 'e') {
		type = 2;
		json += 4;
	}
	if (json[0] == 'f' && json[1] == 'a' && json[2] == 'l' && json[3] == 's' && json[4] == 'e') {
		type = 3;
		json += 5;
	}
	if (type == 0) return ERR_INVALID_TYPE;
	if (is_not_single(json)) return ERR_NOT_SINGLE;
	switch (type) {
		case 1:
			v->type = LEPT_NULL;
			break;
		case 2:
			v->type = LEPT_TRUE;
			break;
		case 3:
			v->type = LEPT_FALSE;
			break;
	}
	return SUCCESS;
}

int lept_parse(lept_value * v, const char * json) {
	while(*json != '\0') {
		if (is_blank(*json)) {
			json++;
			continue;
		} else {
			return parse(v, json);
		}
	}
	return ERR_ALL_BLANK;
}
