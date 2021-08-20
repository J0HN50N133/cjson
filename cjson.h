#ifndef CJSON_H__
#define CJSON_H__


#include <stddef.h>

typedef enum {
    JSON_NULL,
    JSON_FALSE,
    JSON_TRUE,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_type;

enum {
    JSON_PARSE_OK = 0,
    JSON_PARSE_EXPECT_VALUE, // if the json contains only whitespace.
    JSON_PARSE_INVALID_VALUE,
    JSON_PARSE_ROOT_NOT_SINGULAR, // if the json has more character after terminator
    JSON_PARSE_NUMBER_TOO_BIG,
    JSON_PARSE_MISS_QUOTATION_MARK,
    JSON_PARSE_INVALID_STRING_ESCAPE,
    JSON_PARSE_INVALID_STRING_CHAR,
    JSON_PARSE_INVALID_UNICODE_SURROGATE,
    JSON_PARSE_INVALID_UNICODE_HEX,
    JSON_PARSE_MISS_COMMA_OR_SQUARE_BRACKET
};

typedef struct json_value json_value;
struct json_value {
    json_type type;
    union {
        struct { json_value *e; size_t size; } arr; /*array*/
        struct { char *s; size_t len;} str;      /* string */
        double number;                           /* number */
    } val;
};

#define json_val_init(v) do{ (v)-> type = JSON_NULL; } while(0)
#define json_set_null(v) json_val_free(v);

int json_parse(json_value *, const char *);
void json_val_free(json_value *v);
json_type json_get_type(const json_value *v);
void json_set_boolean(json_value *v,int b);
int json_get_boolean(const json_value *v);
void json_set_number(json_value *v, double n);
double json_get_number(const json_value *v);
void json_set_string(json_value *v, const char *s, size_t len);
size_t json_get_string_length(json_value *v);
const char* json_get_string(json_value *v);
size_t json_get_array_size(json_value *v);
json_value * json_get_array_element(json_value *v, size_t index);

typedef struct {
    const char *json;
    char* stack;
    size_t size, top;
} json_context;


#endif
