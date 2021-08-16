#ifndef CJSON_H__
#define CJSON_H__


typedef enum {
    JSON_NULL,
    JSON_FALSE,
    JSON_TURE,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_type;

enum {
    JSON_PARSE_OK = 0,
    JSON_PARSE_EXPECT_VALUE, // if the json contains only whitespace.
    JSON_PARSE_INVALID_VALUE,
    JSON_PARSE_ROOT_NOT_SINGULAR // if the json has more character after terminator
};

typedef struct {
    json_type type;
} json_value;

typedef struct {
    const char *json;
}json_context;
json_type json_get_type(const json_value *);

int json_parse(json_value *, const char *);



#endif
