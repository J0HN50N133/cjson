#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include "cjson.h"

#ifndef JSON_PARSE_STACK_INIT_SIZE
#define JSON_PARSE_STACK_INIT_SIZE 256
#endif

#define EXPECT(c, ch) do{ assert(*c->json == (ch)); c->json++;} while(0)
#define ISWHITE(ch) ((ch) == ' ' || (ch) == '\t' || (ch) == '\n' || (ch) == '\r')
#define ISDIGIT(ch) ('0' <= (ch) && (ch) <= '9')
#define ISHEXDIGIT(ch) (('0' <= (ch) && (ch) <= '9') || ('A' <= (ch) && (ch) <= 'F') || ('a' <= (ch) && (ch) <= 'f'))
#define ISDIGIT1TO9(ch) ('1' <= (ch) && (ch) <= '9')
#define EATDIGIT(n) do{ while(ISDIGIT(*n)) n++; }while(0)
#define PUTC(c, ch) do { *(char *)json_context_push(c, sizeof(char)) = ch; } while(0)
#define RET_ERROR_AND_SET_STACK(c, ret, head) do { (c)->top = (head); return ret; } while(0)

static void json_parse_whitespace(json_context *c);

static int json_parse_value(json_context *c, json_value *v);

static int json_parse_number(json_context *c, json_value *v);

static int json_parse_string(json_context *c, json_value *ptr);

static int json_parse_literal(json_context *c, json_value *v, const char *literal, json_type type);

static void *json_context_pop(json_context *c, size_t size);

static void *json_context_push(json_context *c, size_t size);

static const char *json_parse_hex4(const char *p, unsigned int *u);

static void json_encode_utf8(json_context *c, unsigned int u);

static int json_parse_array(json_context *c, json_value *v);

int json_parse(json_value *v, const char *json) {
        json_context c;
        int parse_result;
        assert(v != NULL);
        c.json = json;
        c.stack = NULL;
        c.size = c.top = 0;
        json_val_init(v);
        parse_result = json_parse_value(&c, v);
        if (parse_result == JSON_PARSE_OK){
                json_parse_whitespace(&c);
                if(*c.json != '\0')
                        parse_result = JSON_PARSE_ROOT_NOT_SINGULAR;
        }
        assert(c.top == 0);
        free(c.stack);
        return parse_result;
}

static int json_parse_value(json_context *c, json_value *v) {
        json_parse_whitespace(c);
        switch (*c->json) {
        case 'n':return json_parse_literal(c, v, "null", JSON_NULL);
        case 't':return json_parse_literal(c, v, "true", JSON_TRUE);
        case 'f':return json_parse_literal(c, v, "false", JSON_FALSE);
        case '\"':return json_parse_string(c, v);
        case '[':return json_parse_array(c, v);
        case '\0':return JSON_PARSE_EXPECT_VALUE;
        default:return json_parse_number(c, v);
        }
}

static void json_parse_whitespace(json_context *c) {
        const char *p = c->json;
        while (ISWHITE(*p))
                p++;
        c->json = p;
}

int json_parse_number(json_context *c, json_value *v) {
        // 负数
        char *ptr = c->json;
        if (*ptr == '-')ptr++;
        // 整数
        if (*ptr == '0'){
                ptr++;
                if(!(*ptr == '.' || ISWHITE(*ptr) || *ptr == '\0'))
                        return JSON_PARSE_ROOT_NOT_SINGULAR;
        } else {
                if (ISDIGIT1TO9(*ptr)) EATDIGIT(ptr);
                else return JSON_PARSE_INVALID_VALUE;
        }
        // 小数
        if (*ptr == '.') {
                ptr++;
                if (!ISDIGIT(*ptr)) return JSON_PARSE_INVALID_VALUE;
                EATDIGIT(ptr);
        }

        // 指数
        if (*ptr == 'e' || *ptr == 'E') {
                ptr++;
                if (*ptr == '+' || *ptr == '-') ptr++;
                if (!ISDIGIT(*ptr)) return JSON_PARSE_INVALID_VALUE;
                EATDIGIT(ptr);
        }
        v->val.number = strtod(c->json, NULL);
        if (errno == ERANGE && (v->val.number == HUGE_VAL || v->val.number == -HUGE_VAL))
                return JSON_PARSE_NUMBER_TOO_BIG;
        c->json = ptr;
        v->type = JSON_NUMBER;
        return JSON_PARSE_OK;
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

static int json_parse_string(json_context *c, json_value *v) {
        size_t head = c->top, len;
        const char *p;
        EXPECT(c, '\"');
        p = c->json;
        unsigned u;
        while (1) {
                char ch = *p++;
                switch (ch) {
                case '\\':
                        switch (*p++) {
                        case '\\':PUTC(c, '\\');break;
                        case '\"':PUTC(c, '\"');break;
                        case 'b':PUTC(c, '\b');break;
                        case 'f':PUTC(c, '\f');break;
                        case 'n':PUTC(c, '\n');break;
                        case 'r':PUTC(c, '\r');break;
                        case 't':PUTC(c, '\t');break;
                        case '/':PUTC(c, '/');break;
                        case 'u':
                                if (!(p = json_parse_hex4(p, &u)))
                                        RET_ERROR_AND_SET_STACK(c, JSON_PARSE_INVALID_UNICODE_HEX, head);
                                if(0xD800 <= u && u <= 0xDBFF){
                                        unsigned h = u;
                                        if(*p++ != '\\')
                                                RET_ERROR_AND_SET_STACK(c, JSON_PARSE_INVALID_UNICODE_SURROGATE, head);
                                        if(*p++ != 'u')
                                                RET_ERROR_AND_SET_STACK(c, JSON_PARSE_INVALID_UNICODE_SURROGATE, head);
                                        if(!(p = json_parse_hex4(p, &u)))
                                                RET_ERROR_AND_SET_STACK(c, JSON_PARSE_INVALID_UNICODE_HEX, head);
                                        if(!(0xDC00 <= u && u <= 0xDFFF))
                                                RET_ERROR_AND_SET_STACK(c, JSON_PARSE_INVALID_UNICODE_SURROGATE, head);
                                        u = 0x10000 + (((h - 0xD800) << 10) | (u - 0xDC00));
                                }
                                json_encode_utf8(c, u);
                                break;
                        default:
                                RET_ERROR_AND_SET_STACK(c, JSON_PARSE_INVALID_STRING_ESCAPE, head);
                        }
                        break;
                case '\"':
                        len = c->top - head;
                        json_set_string(v, (const char *) json_context_pop(c, len), len);
                        c->json = p;
                        return JSON_PARSE_OK;
                case '\0':
                        RET_ERROR_AND_SET_STACK(c, JSON_PARSE_MISS_QUOTATION_MARK, head);
                default:
                        if ((unsigned char) ch < 0x20)
                                RET_ERROR_AND_SET_STACK(c, JSON_PARSE_INVALID_STRING_CHAR, head);
                        PUTC(c, ch);
                }
        }
}


static int json_parse_array(json_context *c, json_value *v) {
        EXPECT(c, '[');
        size_t head = c->top, size = 0;
        int ret;
        json_parse_whitespace(c);
        if (*c->json == ']') {
                c->json++;
                v->type = JSON_ARRAY;
                v->val.arr.size = 0;
                v->val.arr.e = NULL;
                return JSON_PARSE_OK;
        }
        json_value e;
        json_val_init(&e);
        while (1) {
                if ((ret = json_parse_value(c, &e)) != JSON_PARSE_OK) {
                        size_t i;
                        for (i = 0; i < size; i++) json_val_free(json_context_pop(c, sizeof(json_value)));
                        RET_ERROR_AND_SET_STACK(c, ret, head);
                }
                memcpy(json_context_push(c, sizeof(json_value)), &e, sizeof(json_value));
                size++;
                json_parse_whitespace(c);
                if(*c->json == ','){
                        c->json++;
                        json_parse_whitespace(c);
                        if(*c->json == ']'){
                                json_val_free(v);
                                RET_ERROR_AND_SET_STACK(c, JSON_PARSE_INVALID_VALUE, head);
                        }
                } else if (*c->json == ']') {
                        c->json++;
                        v->type = JSON_ARRAY;
                        v->val.arr.size = size;
                        size *= sizeof(json_value);
                        memcpy(v->val.arr.e = (json_value *) malloc(size), json_context_pop(c, size), size);
                        return JSON_PARSE_OK;
                } else
                        RET_ERROR_AND_SET_STACK(c, JSON_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, head);
        }
}

json_type json_get_type(const json_value *v) {
        assert(v != NULL);
        return v->type;
}

double json_get_number(const json_value *v) {
        assert(v != NULL);
        assert(v->type == JSON_NUMBER);
        return v->val.number;
}

void json_set_boolean(json_value *v, int b) {
        assert(v != NULL);
        json_val_free(v);
        if (!b)
                v->type = JSON_FALSE;
        else
                v->type = JSON_TRUE;
}

int json_get_boolean(const json_value *v) {
        assert(v != NULL && (v->type == JSON_TRUE || v->type == JSON_FALSE));
        if (v->type == JSON_TRUE)
                return 1;
        if (v->type == JSON_FALSE)
                return 0;
}

void json_set_number(json_value *v, double n) {
        assert(v != NULL);
        json_val_free(v);
        v->type = JSON_NUMBER;
        v->val.number = n;
}

void json_set_string(json_value *v, const char *s, size_t len) {
        assert(v != NULL && (s != NULL || len == 0));
        json_val_free(v);
        v->val.str.s = (char *) malloc(len + 1);
        memcpy(v->val.str.s, s, len);
        v->val.str.s[len] = '\0';
        v->val.str.len = len;
        v->type = JSON_STRING;
}

size_t json_get_string_length(json_value *v) {
        assert(v != NULL && v->type == JSON_STRING);
        return v->val.str.len;
}

const char *json_get_string(json_value *v) {
        assert(v != NULL && v->type == JSON_STRING);
        return v->val.str.s;
}

void json_val_free(json_value *v) {
        assert(v != NULL);
        if (v->type == JSON_STRING)
                free(v->val.str.s);
        else if (v->type == JSON_ARRAY){
                size_t i, size;
                for (i = 0, size = v->val.arr.size; i < size; i++) {
                        json_val_free(json_get_array_element(v, i));
                }
                free(v->val.arr.e);
        }
        v->type = JSON_NULL;
}

size_t json_get_array_size(json_value *v) {
        assert(v != NULL && v->type == JSON_ARRAY);
        return v->val.arr.size;
}

json_value *json_get_array_element(json_value *v, size_t index) {
        assert(v != NULL && v->type == JSON_ARRAY);
        assert(index < v->val.arr.size);
        return &v->val.arr.e[index];
}

static void *json_context_pop(json_context *c, size_t size) {
        assert(c->top >= size);
        return c->stack + (c->top -= size);
}

void *json_context_push(json_context *c, size_t size) {
        void *ret;
        assert(size > 0);
        if (c->top + size >= c->size) {
                if (c->size == 0)
                        c->size = JSON_PARSE_STACK_INIT_SIZE;
                while (c->top + size >= c->size)
                        c->size += c->size >> 1; // c->size = c->size*1.5
                c->stack = (char *) realloc(c->stack, c->size);
        }
        ret = c->stack + c->top;
        c->top += size;
        return ret;
}

unsigned int hex_to_int(const char c) {
        assert(ISHEXDIGIT(c));
        if (ISDIGIT(c)) return c - '0';
        else if('A' <= c && c <= 'F') return c - 'A' + 10;
        else if('a' <= c && c <= 'f') return c - 'a' + 10;
}

const char *json_parse_hex4(const char *p, unsigned int *u) {
        assert(p != NULL && u != NULL);
        if (ISHEXDIGIT(p[0]) && ISHEXDIGIT(p[1])
            && ISHEXDIGIT(p[2]) && ISHEXDIGIT(p[3])) {
                *u = (hex_to_int(p[0]) << 12) +
                     (hex_to_int(p[1]) << 8)  +
                     (hex_to_int(p[2]) << 4)  +
                     (hex_to_int(p[3]) << 0)  ;
                return p + 4;
        }
        return NULL;
}

void json_encode_utf8(json_context *c, unsigned int u) {
        if(u <= 0x007F){
                PUTC(c, 0xFF & u);
        } else if(u <= 0x07FF){
                PUTC(c, 0xC0 | (0xFF & u>>6));
                PUTC(c, 0x80 | (0x7F & u>>0));
        } else if(u <= 0xFFFF){
                PUTC(c, 0xE0 | (0xFF & u>>12));
                PUTC(c, 0x80 | (0x3F & u>>6));
                PUTC(c, 0x80 | (0x3F & u>>0));
        } else{
                assert(u <= 0x10FFFF);
                PUTC(c, 0xF0 | (0xFF & u>>18));
                PUTC(c, 0x80 | (0x3F & u>>12));
                PUTC(c, 0x80 | (0x3F & u>>6));
                PUTC(c, 0x80 | (0x3F & u>>0));
        }
}
