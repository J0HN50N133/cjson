#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include "cjson.h"

#define EXPECT(c, ch) do{ assert(*c->json == (ch)); c->json++;} while(0)

static int json_parse_null(json_context *c, json_value *v);

static void json_parse_whitespace(json_context *c);

static int json_parse_value(json_context *c, json_value *v);

static int json_parse_true(json_context *c, json_value *v);

static int json_parse_false(json_context *c, json_value *v);

static int json_parse_number(json_context *c, json_value *v);

static int validate_number(const char *ptr, char **end);

static int json_parse_literal(json_context *c, json_value *v, const char *literal, json_type type);

int json_parse(json_value *v, const char *json) {
        json_context c;
        int parse_result;

        assert(v != NULL);

        c.json = json;
        v->type = JSON_NULL;
        json_parse_whitespace(&c);
        parse_result = json_parse_value(&c, v);
        if (parse_result == JSON_PARSE_OK) {
                json_parse_whitespace(&c);
                if (*c.json != '\0')
                        return JSON_PARSE_ROOT_NOT_SINGULAR;
        }
        return parse_result;
}

static int json_parse_value(json_context *c, json_value *v) {
        switch (*c->json) {
                case 'n':
                        return json_parse_null(c, v);
                case 't':
                        return json_parse_true(c, v);
                case 'f':
                        return json_parse_false(c, v);
                case '\0':
                        return JSON_PARSE_EXPECT_VALUE;
                default:
                        return json_parse_number(c, v);
        }
}

static void json_parse_whitespace(json_context *c) {
        const char *p = c->json;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
                p++;
        c->json = p;
}


static int json_parse_null(json_context *c, json_value *v) {
        return json_parse_literal(c, v, "null", JSON_NULL);
}

int json_parse_true(json_context *c, json_value *v) {
        return json_parse_literal(c, v, "true", JSON_TRUE);
}

int json_parse_false(json_context *c, json_value *v) {
        return json_parse_literal(c, v, "false", JSON_FALSE);
}

int json_parse_number(json_context *c, json_value *v) {
        char *end;

        if (!validate_number(c->json, &end))
                return JSON_PARSE_INVALID_VALUE;
        v->number = strtod(c->json, NULL);
        if (errno == ERANGE && (v->number == HUGE_VAL || v->number == -HUGE_VAL))
                return JSON_PARSE_NUMBER_TOO_BIG;
        c->json = end;
        v->type = JSON_NUMBER;

        return JSON_PARSE_OK;
}

#define ISDIGIT(ch) ('0' <= (ch) && (ch) <= '9')
#define ISDIGIT1TO9(ch) ('1' <= (ch) && (ch) <= '9')
#define EATDIGIT(n) do{ while(ISDIGIT(*n)) n++; }while(0)

static int validate_number(const char *ptr, char **end) {
        assert(ptr != NULL);
        // 负数
        if (*ptr == '-')ptr++;
        // 整数
        if (*ptr == '0')ptr++;
        else {
                if (ISDIGIT1TO9(*ptr)) EATDIGIT(ptr);
                else return 0;
        }
        // 小数
        if (*ptr == '.') {
                ptr++;
                if (!ISDIGIT(*ptr)) return 0;
                EATDIGIT(ptr);
        }

        // 指数
        if (*ptr == 'e' || *ptr == 'E') {
                ptr++;
                if (*ptr == '+' || *ptr == '-') ptr++;
                if (!ISDIGIT(*ptr)) return 0;
                EATDIGIT(ptr);
        }

        *end = ptr;
        return 1;
}

static int json_parse_literal(json_context *c, json_value *v, const char *literal, json_type type) {
        size_t i;
        for (i = 0; literal[i]; i++)
                if (c->json[i] != literal[i])
                        return JSON_PARSE_INVALID_VALUE;
        c->json += i;
        v->type = type;
        return JSON_PARSE_OK;
}

json_type json_get_type(const json_value *v) {
        return v->type;
}

double json_get_number(const json_value *v) {
        assert(v != NULL);
        assert(v->type == JSON_NUMBER);
        return v->number;
}
