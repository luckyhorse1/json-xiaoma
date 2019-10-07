#include "leptjson.h"
#include <assert.h>
#include <stdlib.h> /* NULL, strtod(), malloc(), realloc(), free() */
#include <errno.h> /* errno, ERANGE */
#include <math.h> /* HUGE_VAL */
#include <string.h> /* memcpy() */
#include <stdio.h>


#define EXPECT(c, ch) do { assert(*c->json == (ch) ); c->json++; } while(0) 
#define ISDIGIT(ch) (ch >= '0' && ch <= '9')
#define ISDIGIT1TO9(ch) (ch >= '1' && ch <= '9')

#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

#ifndef LEPT_PARSE_STRINGIFY_INIT_SIZE
#define LEPT_PARSE_STRINGIFY_INIT_SIZE 256
#endif

#define PUTC(c, ch) do { *(char*)lept_context_push(c, sizeof(char)) = (ch); } while(0)
#define PUTS(c, s, len) memcpy(lept_context_push(c, len), s, len)

#define STRING_ERROR(ret) do { c->top = head; return ret; } while(0)

typedef struct {
	const char * json;
	char * stack;
	size_t size, top;
} lept_context;

static void* lept_context_push(lept_context * c, int size) {
	void * ret;
	assert(size>0);
	if (size + c->top >= c->size) {
		if (c->size == 0) 
			c->size = LEPT_PARSE_STACK_INIT_SIZE;
		while(size + c->top >= c->size)
			c->size += c->size >> 1;
		c->stack = (char*)realloc(c->stack, c->size);
	}
	ret = c->stack + c->top; /*注意这里为什么ret写在前面，c->top写在后面。*/
	c->top += size;
	return ret;
}

static void* lept_context_pop(lept_context * c, size_t size) {
	assert(size <= c->top);
	return c->stack + (c->top -= size); /* 他这个弹出的是后进入栈的元素. */
}

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
			return LEPT_PARSE_INVALID_VALUE;
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
		if (!ISDIGIT1TO9(*p)) return LEPT_PARSE_INVALID_VALUE;
		for (p++; ISDIGIT(*p); p++);
	}
	if (*p == '.') {
		p++;
		if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
		for (p++; ISDIGIT(*p); p++);
	}
	if (*p == 'e' || *p == 'E') {
		p++;
		if (*p == '+' || *p == '-') p++;
		if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
		for (p++; ISDIGIT(*p); p++);
	}
	errno = 0;
	v->u.n = strtod(c->json, NULL);
	if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL))
		return LEPT_PARSE_NUMBER_TOO_BIG;
	c->json = p;
	v->type = LEPT_NUMBER;
	return LEPT_PARSE_OK;
}

static const char *  lept_parse_hex4(const char * p, unsigned * u) {
	size_t i;
	*u = 0;
	for (i=0; i<4; i++) {
		char ch = *p++;
		*u <<= 4;
		if (ch >= '0' && ch <= '9') *u |= ch - '0';
		else if (ch >= 'A' && ch <= 'F') *u |= ch - 'A' + 10;
		else if (ch >= 'a' && ch <= 'f') *u |= ch - 'a' + 10;
		else return NULL;
	}
	return p;
}

static void lept_encode_utf8(lept_context * c, unsigned u) {
	if (u <= 0x7F){
        PUTC(c, u & 0xFF);
	} 
    else if (u <= 0x7FF) {
        PUTC(c, 0xC0 | ((u >> 6) & 0xFF));
        PUTC(c, 0x80 | ( u       & 0x3F));
    }
    else if (u <= 0xFFFF) {
        PUTC(c, 0xE0 | ((u >> 12) & 0xFF));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
    else {
        assert(u <= 0x10FFFF);
        PUTC(c, 0xF0 | ((u >> 18) & 0xFF));
        PUTC(c, 0x80 | ((u >> 12) & 0x3F));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }	
}

static int lept_parse_string_raw(lept_context * c, char ** str, size_t * len) {
	size_t head = c->top;
	const char * p;
	unsigned u, u2;
	EXPECT(c, '\"');
	p = c->json;
	for(;;) {
		char ch = *p++;
		switch(ch){
			case '\"':
				*len = c->top - head;
				c->json = p;
				*str = (char *)lept_context_pop(c, *len);
				return LEPT_PARSE_OK;
			case '\0':
				STRING_ERROR(LEPT_PARSE_MISS_QUOTATION_MARK);
			case '\\':
				switch(*p++) {
					case '\"': PUTC(c, '\"'); break;
					case '/': PUTC(c, '/'); break;
					case '\\': PUTC(c, '\\'); break;
					case 'b': PUTC(c, '\b'); break;
					case 'f': PUTC(c, '\f'); break;
					case 'n': PUTC(c, '\n'); break;
					case 'r': PUTC(c, '\r'); break;
					case 't': PUTC(c, '\t'); break;
					case 'u':
						if (!(p=lept_parse_hex4(p, &u))) {
							STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
						}
						if (u >= 0xD800 && u<=0xDBFF) {
							if (*p++ != '\\')
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            if (*p++ != 'u') {
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
							}
                            if (!(p = lept_parse_hex4(p, &u2)))
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                            if (u2 < 0xDC00 || u2 > 0xDFFF)
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
							u = (((u-0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
						}
						lept_encode_utf8(c, u);
						break;
					default:
						STRING_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE);
				}
				break;
			default:
				if ((unsigned char)ch < 0x20) {
					STRING_ERROR(LEPT_PARSE_INVALID_STRING_CHAR);
				}
				PUTC(c, ch);
		}
	}
}

static int lept_parse_string (lept_value * v, lept_context * c) {
	int ret;
	char * str;
	size_t len;
	if ((ret = lept_parse_string_raw(c, &str, &len)) != LEPT_PARSE_OK)
		return ret;
	lept_set_string(v, str, len);
	return LEPT_PARSE_OK;
}

static int lept_parse_value(lept_value * v, lept_context * c);

static int lept_parse_array(lept_value * v, lept_context * c) {
	int ret;
	size_t size = 0, i = 0;
	EXPECT(c, '[');
	lept_parse_whitespace(c);
	if (*c->json == ']') {
		c->json++;
		v->type = LEPT_ARRAY;
		v->u.a.size = 0;
		v->u.a.e = NULL;
		return LEPT_PARSE_OK;
	}
	for(;;){
		lept_value e;
		lept_init(&e);
		lept_parse_whitespace(c);
		if ((ret = lept_parse_value(&e, c)) != LEPT_PARSE_OK)
			break;
		memcpy(lept_context_push(c, sizeof(lept_value)), &e, sizeof(lept_value));
		size++;
		lept_parse_whitespace(c);
		if (*c->json == ',') {
			c->json++;
		} else if (*c->json == ']') {
			c->json++;
			v->type = LEPT_ARRAY;
			v->u.a.size = size;
			size *= sizeof(lept_value);
			memcpy(v->u.a.e = (lept_value *)malloc(size), lept_context_pop(c, size), size);
			return LEPT_PARSE_OK;
		} else {
			ret = LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
			break;
		}
	}
	for (i=0; i < size; i++) {
		lept_free((lept_value *)lept_context_pop(c, sizeof(lept_value)));
	}
	return ret;
}

static int lept_parse_object(lept_value * v, lept_context * c) {
    size_t i, size = 0;
    lept_member m;
    int ret;
	m.k = NULL;
	m.klen = 0;
    EXPECT(c, '{');
    lept_parse_whitespace(c);
    if (*c->json == '}') {
        c->json++;
        v->type = LEPT_OBJECT;
        v->u.o.m = NULL;
        v->u.o.size = 0;
        return LEPT_PARSE_OK;
    }
    for (;;) {
        char* str;
        lept_init(&m.v);
        /* parse key */
        if (*c->json != '\"') {
            ret = LEPT_PARSE_MISS_KEY;
            break;
        }
        if ((ret = lept_parse_string_raw(c, &str, &m.klen)) != LEPT_PARSE_OK)
            break;
        memcpy(m.k = (char*)malloc(m.klen + 1), str, m.klen);
        m.k[m.klen] = '\0';
        /* parse ws colon ws */
        lept_parse_whitespace(c);
        if (*c->json != ':') {
            ret = LEPT_PARSE_MISS_COLON;
            break;
        }
        c->json++;
        lept_parse_whitespace(c);
        /* parse value */
        if ((ret = lept_parse_value(&m.v, c)) != LEPT_PARSE_OK)
            break;
        memcpy(lept_context_push(c, sizeof(lept_member)), &m, sizeof(lept_member));
        size++;
        m.k = NULL; /* ownership is transferred to member on stack */
        /* parse ws [comma | right-curly-brace] ws */
        lept_parse_whitespace(c);
        if (*c->json == ',') {
            c->json++;
            lept_parse_whitespace(c);
        }
        else if (*c->json == '}') {
            v->u.o.size = size;
            c->json++;
            v->type = LEPT_OBJECT;
            size *= sizeof(lept_member);
            memcpy(v->u.o.m = (lept_member*)malloc(size), lept_context_pop(c, size), size);
            return LEPT_PARSE_OK;
        }
        else {
            ret = LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET;
            break;
        }
    }
    /* Pop and free members on the stack */
    free(m.k);
    for (i = 0; i < size; i++) {
        lept_member* m = (lept_member*)lept_context_pop(c, sizeof(lept_member));
        free(m->k);
        lept_free(&m->v);
    }
    v->type = LEPT_NULL;
    return ret;
}


static int lept_parse_value(lept_value * v, lept_context * c) {
	switch (*c->json) {
		case 'n':
			return lept_parse_literal(v, c, "null", LEPT_NULL);
		case 't':
			return lept_parse_literal(v, c, "true", LEPT_TRUE);
		case 'f':
			return lept_parse_literal(v, c, "false", LEPT_FALSE);
		case '\"':
			return lept_parse_string(v, c);
		case '[':
			return lept_parse_array(v, c);
		case '{':
			return lept_parse_object(v, c);
		default:
			return lept_parse_number(v, c);	
		case '\0':
			return LEPT_PARSE_ALL_BLANK;
	}
}

int lept_parse(lept_value * v, const char * json) {
	lept_context c;
	int ret;
	assert(v != NULL);
	c.json = json;
	c.top = c.size = 0;
	c.stack = NULL;
	lept_init(v);
	lept_parse_whitespace(&c);
	if ((ret = lept_parse_value(v, &c)) == LEPT_PARSE_OK) {
		lept_parse_whitespace(&c);
		if (*c.json != '\0') {
			v->type = LEPT_NULL;
			ret = LEPT_PARSE_NOT_SINGLE;
		}
	}
	assert(c.top == 0);
	free(c.stack);
	return ret;
}

lept_type lept_get_type(const lept_value * v) {
	assert(v != NULL);
	return v->type;
}

double lept_get_number(const lept_value * v) {
	assert( v!=NULL && v->type == LEPT_NUMBER);
	return v->u.n;
}

void lept_free(lept_value * v) {
	assert(v!=NULL);
	size_t i = 0;
	if (v->type == LEPT_STRING) {
		free(v->u.s.s);
	}
	if (v->type == LEPT_ARRAY) {
		for (i=0; i < v->u.a.size; i++){
			lept_free(v->u.a.e + i);
		}
		free(v->u.a.e);
	}
	v->type = LEPT_NULL;
}

void lept_set_string(lept_value * v, const char * s, size_t len) {
	assert(v!=NULL && (s!=NULL || len == 0));
	lept_free(v);
	v->u.s.s  = (char*)malloc(len+1);
	memcpy(v->u.s.s, s, len);
	v->u.s.s[len] = '\0';
	v->u.s.len = len;
	v->type = LEPT_STRING;
}

const char* lept_get_string(const lept_value * v) {
	assert(v!=NULL && v->type == LEPT_STRING);
	return v->u.s.s;
}

size_t lept_get_string_length(const lept_value * v) {
	assert(v!=NULL && v->type == LEPT_STRING);
	return v->u.s.len;
}

void lept_set_boolean(lept_value * v, int b) {
	assert(v!=NULL);
	lept_free(v);
	v->type = b ? LEPT_TRUE : LEPT_FALSE;
}

size_t lept_get_array_size(const lept_value * v) {
	assert(v!=NULL && v->type == LEPT_ARRAY);
	return v->u.a.size;
}

const lept_value * lept_get_array_element(const lept_value * v, size_t index) {
	assert(v!=NULL && v->type == LEPT_ARRAY);
	assert(index < v->u.a.size);
	return v->u.a.e + index;
}

size_t lept_get_object_size(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    return v->u.o.size;
}

const char* lept_get_object_key(const lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    assert(index < v->u.o.size);
    return v->u.o.m[index].k;
}

size_t lept_get_object_key_length(const lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    assert(index < v->u.o.size);
    return v->u.o.m[index].klen;
}

lept_value* lept_get_object_value(const lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    assert(index < v->u.o.size);
    return &v->u.o.m[index].v;
}

#define LEPT_KEY_NOT_EXIST ((size_t) - 1)

size_t lept_find_object_index(const lept_value * v, const char * key, size_t klen) {
	size_t i;
	assert(v!=NULL && v->type == LEPT_OBJECT);
	for (i=0; i<v->u.o.size; i++) {
		if (klen == v->u.o.m[i].klen && memcmp(v->u.o.m[i].k, key, klen) == 0)
			return i;
	}
	return LEPT_KEY_NOT_EXIST;
}

lept_value * lept_find_object_value(const lept_value * v, const char * key, size_t klen) {
	size_t index = lept_find_object_index(v, key, klen);
	return index == LEPT_KEY_NOT_EXIST ? NULL : &v->u.o.m[index].v;
}

int lept_is_equal(const lept_value * v1, const lept_value * v2) {
	size_t i;
	lept_value * temp;
	assert(v1 != NULL && v2 != NULL);
	if (v1->type != v2->type)
		return 0;
	switch (v1->type) {
		case LEPT_NUMBER:
			return v1->u.n == v2->u.n;
		case LEPT_STRING:
			return v1->u.s.len == v2->u.s.len && memcmp(v1->u.s.s, v2->u.s.s, v1->u.s.len) == 0;
		case LEPT_ARRAY:
			if (v1->u.a.size != v2->u.a.size) return 0;
			for (i=0; i<v1->u.a.size; i++) {
				if (!lept_is_equal(&v1->u.a.e[i], &v2->u.a.e[i]))
					return 0;
			}
			return 1;
		case LEPT_OBJECT:
			if (v1->u.o.size != v2->u.o.size) return 0;
			for (i=0; i<v1->u.o.size; i++) {
				temp = lept_find_object_value(v2, v1->u.o.m[i].k, v1->u.o.m[i].klen);
				if (!lept_is_equal(&v1->u.o.m[i].v, temp))
					return 0;
			}
			return 1;
		default:
			return 1;
	}
}

void lept_copy(lept_value * dst, const lept_value * src) {
	size_t i, len;
	assert(src != NULL && dst != NULL && src != dst);
	lept_free(dst);
	dst->type = src->type;
	switch (src->type) {
		case LEPT_NUMBER:
			dst->u.n = src->u.n;
			break;
		case LEPT_STRING:
			lept_set_string(dst, src->u.s.s, src->u.s.len);
		case LEPT_ARRAY:
			dst->u.a.size = src->u.a.size;
			dst->u.a.e = (lept_value *)malloc(dst->u.a.size * sizeof(lept_value));
			for (i=0; i<dst->u.a.size; i++) {
				lept_copy(&dst->u.a.e[i], &src->u.a.e[i]);
			}
			break;
		case LEPT_OBJECT:
			dst->u.o.size = src->u.o.size;
			dst->u.o.m = (lept_member *)malloc(dst->u.o.size * sizeof(lept_member));
			for (i=0; i<dst->u.o.size; i++) {
				len = dst->u.o.m[i].klen = src->u.o.m[i].klen;
				memcpy(dst->u.o.m[i].k = malloc(len+1), src->u.o.m[i].k, len);
				dst->u.o.m[i].k[len] = '\0';
				lept_copy(&dst->u.o.m[i].v, &src->u.o.m[i].v);
			}
			break;
		default:
			break;
	}
}

void lept_move(lept_value * dst, lept_value * src) {
	assert(dst != NULL && src != NULL);
	lept_free(dst);
	memcpy(dst, src, sizeof(lept_value));
	lept_init(src);
}

void lept_swap(lept_value *v1, lept_value * v2) {
	assert(v1 != NULL && v2 != NULL);
	if (v1 != v2) {
		lept_value temp;
		memcpy(&temp, v1, sizeof(lept_value));
		memcpy(v1, v2, sizeof(lept_value));
		memcpy(v2, &temp, sizeof(lept_value));
	}
}

#if 0
// Unoptimized
static void lept_stringify_string(lept_context* c, const char* s, size_t len) {
    size_t i;
    assert(s != NULL);
    PUTC(c, '"');
    for (i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)s[i];
        switch (ch) {
            case '\"': PUTS(c, "\\\"", 2); break;
            case '\\': PUTS(c, "\\\\", 2); break;
            case '\b': PUTS(c, "\\b",  2); break;
            case '\f': PUTS(c, "\\f",  2); break;
            case '\n': PUTS(c, "\\n",  2); break;
            case '\r': PUTS(c, "\\r",  2); break;
            case '\t': PUTS(c, "\\t",  2); break;
            default:
                if (ch < 0x20) {
                    char buffer[7];
                    sprintf(buffer, "\\u%04X", ch);
                    PUTS(c, buffer, 6);
                }
                else
                    PUTC(c, s[i]);
        }
    }
    PUTC(c, '"');
}
#else
static void lept_stringify_string(lept_context* c, const char* s, size_t len) {
    static const char hex_digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    size_t i, size;
    char* head, *p;
    assert(s != NULL);
    p = head = lept_context_push(c, size = len * 6 + 2); /* "\u00xx..." */
    *p++ = '"';
    for (i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)s[i];
        switch (ch) {
            case '\"': *p++ = '\\'; *p++ = '\"'; break;
            case '\\': *p++ = '\\'; *p++ = '\\'; break;
            case '\b': *p++ = '\\'; *p++ = 'b';  break;
            case '\f': *p++ = '\\'; *p++ = 'f';  break;
            case '\n': *p++ = '\\'; *p++ = 'n';  break;
            case '\r': *p++ = '\\'; *p++ = 'r';  break;
            case '\t': *p++ = '\\'; *p++ = 't';  break;
            default:
                if (ch < 0x20) {
                    *p++ = '\\'; *p++ = 'u'; *p++ = '0'; *p++ = '0';
                    *p++ = hex_digits[ch >> 4];
                    *p++ = hex_digits[ch & 15];
                }
                else
                    *p++ = s[i];
        }
    }
    *p++ = '"';
    c->top -= size - (p - head);
}
#endif

static void lept_stringify_number(lept_context * c, const lept_value * v) {
	char* buffer = lept_context_push(c, 32);
    int length = sprintf(buffer, "%.17g", v->u.n);
    c->top -= 32 - length;
	/*c->top -= 32 - sprintf(lept_context_push(c, 32), "%.17g", v->u.n);*/
}

static void lept_stringify_value(lept_context* c, const lept_value* v) {
    size_t i;
    switch (v->type) {
        case LEPT_NULL:   PUTS(c, "null",  4); break;
        case LEPT_FALSE:  PUTS(c, "false", 5); break;
        case LEPT_TRUE:   PUTS(c, "true",  4); break;
		case LEPT_NUMBER: lept_stringify_number(c, v);break;
        case LEPT_STRING: lept_stringify_string(c, v->u.s.s, v->u.s.len); break;
        case LEPT_ARRAY:
            PUTC(c, '[');
            for (i = 0; i < v->u.a.size; i++) {
                if (i > 0)
                    PUTC(c, ',');
                lept_stringify_value(c, &v->u.a.e[i]);
            }
            PUTC(c, ']');
            break;
        case LEPT_OBJECT:
            PUTC(c, '{');
            for (i = 0; i < v->u.o.size; i++) {
                if (i > 0)
                    PUTC(c, ',');
                lept_stringify_string(c, v->u.o.m[i].k, v->u.o.m[i].klen);
                PUTC(c, ':');
                lept_stringify_value(c, &v->u.o.m[i].v);
            }
            PUTC(c, '}');
            break;
        default: assert(0 && "invalid type");
    }
}

char* lept_stringify(const lept_value* v, size_t* length) {
    lept_context c;
    assert(v != NULL);
    c.stack = (char*)malloc(c.size = LEPT_PARSE_STRINGIFY_INIT_SIZE);
    c.top = 0;
    lept_stringify_value(&c, v);
    if (length)
        *length = c.top;
    PUTC(&c, '\0');
    return c.stack;
}


