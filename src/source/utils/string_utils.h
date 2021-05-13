#ifndef GENERIC_STRING_UTILS
#define GENERIC_STRING_UTILS

#include <stdlib.h>
#include <string.h>

typedef struct string_struct {
	char* str;
	size_t len;	//length
	size_t cap;	//capacity
} string_t;

int str_constr(string_t* str);

int str_destr(string_t* str);

size_t str_len(const string_t* str);

size_t str_capacity(const string_t* str);

int str_resize(string_t* str, size_t newlen);

int str_reserve(string_t* str, size_t newcapacity);

int str_clear(string_t* str);

int str_empty(const string_t* str);

const char* str_cstr(const string_t* str);

const char* str_at_const(const string_t* str, size_t pos);
char* str_at(string_t* str, size_t pos);

int str_assign(string_t* dest, const string_t* src);
int str_assign_cstr(string_t* dest, const char* src);

int str_append(string_t* dest, const string_t* src);
int str_append_cstr(string_t* dest, const char* src);

int str_push_back(string_t* str, char ch);

int str_insert(string_t* dest, size_t pos, const string_t* src);
int str_erase(string_t* dest, size_t pos, size_t count);
int str_replace(string_t* dest, size_t pos, size_t count, const string_t* src);

int str_swap(string_t* src1, string_t* src2);

ssize_t str_find_char(const string_t* str, char ch, size_t pos);
ssize_t str_find_cstr(const string_t* str, const char* cstr, size_t pos);
ssize_t str_find_str(const string_t* str, const string_t* src, size_t pos);

ssize_t str_rfind_char(const string_t* str, char ch, size_t pos);
ssize_t str_rfind_cstr(const string_t* str, const char* cstr, size_t pos);
ssize_t str_rfind_str(const string_t* str, const string_t* src, size_t pos);

ssize_t str_find_firstof(const string_t* str, const char* chars, size_t pos);
ssize_t str_find_lastof(const string_t* str, const char* chars, size_t pos);
ssize_t str_find_firstnotof(const string_t* str, const char* chars, size_t pos);
ssize_t str_find_lastnotof(const string_t* str, const char* chars, size_t pos);

int str_substr(string_t* dest, const string_t* src, size_t pos, size_t len);

int str_cmp(const string_t* l, const string_t* r);

#endif
