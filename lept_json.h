typedef enum {LEPT_NULL, LEPT_TRUE, LEPT_FALSE} lept_type;
typedef struct {
	lept_type type;
} lept_value;

typedef enum {SUCCESS, ERR_ALL_BLANK, ERR_INVALID_TYPE, ERR_NOT_SINGLE} parse_result;

int letp_parse(lept_value * v, const char * json);

