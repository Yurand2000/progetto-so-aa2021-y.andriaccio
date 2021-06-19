#include "string_utils.h"

#include <errno.h>
#include "../errset.h"

#ifndef NDEBUG
	#define PTRSTRCHECK(str) PTRCHECKERRSET((str), EINVAL, -1)
	#define PTRSTRCHECKNULL(str) PTRCHECKERRSET((str), EINVAL, NULL)
#else
	#define PTRSTRCHECK(str) ((void) 0)
	#define PTRSTRCHECKNULL(str) ((void) 0)
#endif

int __realloc(string_t* str, size_t newsize)
{
	char* temp;
	REALLOC( temp, (str->str), (newsize * sizeof(char)) );
	str->cap = newsize - 1;
	if(str->len > str->cap)
	{
		str->len = str->cap;
		(str->str)[str->cap] = '\0';
	}
	str->str = temp;
	return 0;
}

int str_constr(string_t* str)
{
	PTRSTRCHECK(str);
	str->str = NULL;
	str->len = 0;
	str->cap = 0;
	return 0;
}

int str_destr(string_t* str)
{
	PTRSTRCHECK(str);
	free(str->str);
	str->str = NULL;
	str->len = 0;
	str->cap = 0;
	return 0;
}

size_t str_len(const string_t* str)
{
	PTRSTRCHECK(str);
	return str->len;
}

size_t str_capacity(const string_t* str)
{
	PTRSTRCHECK(str);
	return str->cap;
}

int str_resize(string_t* str, size_t newlen)
{
	PTRSTRCHECK(str);
	ERRCHECK( __realloc(str, (newlen+1)) );
	if(str->len < newlen)
	{
		memset(&str->str[str->len], 0, newlen - str->len);
	}
	return 0;
}

int str_reserve(string_t* str, size_t newcapacity)
{
	PTRSTRCHECK(str);
	ERRCHECK( __realloc(str, (newcapacity+1)) );
	return 0;
}

int str_clear(string_t* str)
{
	PTRSTRCHECK(str);
	str->len = 0;
	return 0;
}

int str_empty(const string_t* str)
{
	PTRSTRCHECK(str);
	return str->len == 0;	
}

const char* str_cstr(const string_t* str)
{
	PTRSTRCHECKNULL(str);
	return str->str;
}

const char* str_at_const(const string_t* str, size_t pos)
{
	PTRSTRCHECKNULL(str);
	return &((str->str)[pos]);
}

char* str_at(string_t* str, size_t pos)
{
	PTRSTRCHECKNULL(str);
	return &((str->str)[pos]);
}

int str_assign(string_t* dest, const string_t* src)
{
	PTRSTRCHECK(dest);
	PTRSTRCHECK(src);
	if(dest->cap < src->len)
		ERRCHECK( __realloc(dest, src->len + 1) );
	
	strncpy(dest->str, src->str, src->len);
	dest->str[src->len] = '\0';
	dest->len = src->len;
	return 0;
}

int str_assign_cstr(string_t* dest, const char* src)
{
	PTRSTRCHECK(dest);
	PTRSTRCHECK(src);
	size_t len = strlen(src);
	if(dest->cap < len)
		ERRCHECK( __realloc(dest, len + 1) );
	
	strncpy(dest->str, src, len);
	dest->str[len] = '\0';
	dest->len = len;
	return 0;
}

int str_append(string_t* dest, const string_t* src)
{
	PTRSTRCHECK(dest);
	PTRSTRCHECK(src);
	if(dest->cap < dest->len + src->len)
		ERRCHECK( __realloc(dest, dest->len + src->len + 1) );
	
	strncpy(dest->str + dest->len, src->str, src->len);
	dest->str[dest->cap] = '\0';
	dest->len += src->len;
	return 0;
}

int str_append_cstr(string_t* dest, const char* src)
{
	PTRSTRCHECK(dest);
	PTRSTRCHECK(src);
	size_t len = strlen(src);
	if(dest->cap < dest->len + len)
		ERRCHECK( __realloc(dest, dest->len + len + 1) );
	
	strncpy(dest->str + dest->len, src, len);
	dest->str[dest->cap] = '\0';
	dest->len += len;
	return 0;
}

int str_push_back(string_t* str, char ch)
{
	PTRSTRCHECK(str);
	if(str->len >= str->cap)
		ERRCHECK( __realloc(str, str->cap + 10) );

	str->str[str->len] = ch;
	str->len++;
	str->str[str->len] = '\0';
	return 0;
}

int str_insert(string_t* dest, size_t pos, const string_t* src)
{
	PTRSTRCHECK(src);
	PTRSTRCHECK(dest);
	if(pos >= dest->len) ERRSET(EINVAL, -1);
	if(dest->cap < dest->len + src->len)
		ERRCHECK( __realloc(dest, dest->len + src->len + 1) );
	
	memmove(dest->str + pos + src->len, dest->str + pos, src->len + 1);
	strncpy(dest->str + pos, src->str, src->len);
	return 0;
}

int str_erase(string_t* dest, size_t pos, size_t count)
{
	PTRSTRCHECK(dest);
	if(pos >= dest->len) ERRSET(EINVAL, -1);
	if(count == 0 || pos + count > dest->len)
		count = dest->len - pos;

	size_t len = strlen(dest->str + pos + count) + 1;	
	memmove(dest->str + pos, dest->str + pos + count, len);
	return 0;
}

int str_replace(string_t* dest, size_t pos, size_t count, const string_t* src)
{
	ERRCHECK(str_erase(dest, pos, count));
	ERRCHECK(str_insert(dest, pos, src));
	return 0;
}

int str_swap(string_t* src1, string_t* src2)
{
	PTRSTRCHECK(src1);
	PTRSTRCHECK(src2);

	string_t temp;
	temp.str = src1->str;
	temp.len = src1->len;
	temp.cap = src1->cap;

	src1->str = src2->str;
	src1->len = src2->len;
	src1->cap = src2->cap;

	src2->str = temp.str;
	src2->len = temp.len;
	src2->cap = temp.cap;
	return 0;
}

ssize_t str_find_char(const string_t* str, char ch, size_t pos)
{
	PTRSTRCHECK(str);
	if(pos >= str->len) ERRSET(EINVAL, -1);
	for(ssize_t i = pos; i < str->len; i++)
	{
		if(str->str[i] == ch) return i;
	}
	return str->len;
}

ssize_t str_find_cstr(const string_t* str, const char* cstr, size_t pos)
{
	PTRSTRCHECK(str);
	if(pos >= str->len) ERRSET(EINVAL, -1);
	size_t len = strlen(cstr);
	for(ssize_t i = pos; i < str->len; i++)
	{
		if(str->str[i] == cstr[0] &&
		   strncmp(str->str + i, cstr, len) == 0)
			return i;
	}
	return str->len;
}

ssize_t str_find_str(const string_t* str, const string_t* src, size_t pos)
{
	PTRSTRCHECK(str);
	if(pos >= str->len) ERRSET(EINVAL, -1);
	for(ssize_t i = pos; i < str->len; i++)
	{
		if(str->str[i] == src->str[0] &&
		   strncmp(str->str + i, src->str, src->len) == 0)
			return i;
	}
	return str->len;
}

ssize_t str_rfind_char(const string_t* str, char ch, size_t pos)
{
	PTRSTRCHECK(str);
	if(pos >= str->len) ERRSET(EINVAL, -1);
	for(ssize_t i = pos; i >= 0; i--)
	{
		if(str->str[i] == ch) return i;
	}
	return str->len;
}

ssize_t str_rfind_cstr(const string_t* str, const char* cstr, size_t pos)
{
	PTRSTRCHECK(str);
	if(pos >= str->len) ERRSET(EINVAL, -1);
	size_t len = strlen(cstr);
	for(ssize_t i = pos; i >= 0; i--)
	{
		if(str->str[i] == cstr[0] &&
		   strncmp(str->str + i, cstr, len) == 0)
			return i;
	}
	return str->len;
}

ssize_t str_rfind_str(const string_t* str, const string_t* src, size_t pos)
{
	PTRSTRCHECK(str);
	if(pos >= str->len) ERRSET(EINVAL, -1);
	for(ssize_t i = pos; i >= 0; i--)
	{
		if(str->str[i] == src->str[0] &&
		   strncmp(str->str + i, src->str, src->len) == 0)
			return i;
	}
	return str->len;
}

ssize_t str_find_firstof(const string_t* str, const char* chars, size_t pos)
{
	PTRSTRCHECK(str);
	size_t len = strlen(chars);
	for(ssize_t i = pos; i < str->len; i++)
	{
		for(size_t j = 0; j < len; j++)
			if(str->str[i] == chars[j]) return i;
	}
	return str->len;
}

ssize_t str_find_lastof(const string_t* str, const char* chars, size_t pos)
{
	PTRSTRCHECK(str);
	size_t len = strlen(chars);
	for(ssize_t i = pos; i >= 0; i--)
	{
		for(size_t j = 0; j < len; j++)
			if(str->str[i] == chars[j]) return i;
	}
	return str->len;
}

ssize_t str_find_firstnotof(const string_t* str, const char* chars, size_t pos)
{
	PTRSTRCHECK(str);
	size_t len = strlen(chars);
	int pres;
	for(ssize_t i = pos; i < str->len; i++)
	{
		pres = 0;
		for(size_t j = 0; j < len && pres == 0; j++)
			if(str->str[i] == chars[j]) pres = 1;

		if(pres == 0) return i;
	}
	return str->len;
}

ssize_t str_find_lastnotof(const string_t* str, const char* chars, size_t pos)
{
	PTRSTRCHECK(str);
	size_t len = strlen(chars);
	int pres;
	for(ssize_t i = pos; i >= 0; i--)
	{
		pres = 0;
		for(size_t j = 0; j < len && pres == 0; j++)
			if(str->str[i] == chars[j]) pres = 1;

		if(pres == 0) return i;
	}
	return str->len;
}

int str_substr(string_t* dest, const string_t* src, size_t pos, size_t len)
{
	PTRSTRCHECK(dest);
	PTRSTRCHECK(src);
	if(dest->cap < len)
		ERRCHECK( __realloc(dest, len) );
	
	strncpy(dest->str, src->str + pos, len);
	return 0;
}

int str_cmp(const string_t* l, const string_t* r)
{
	return strcmp(l->str, r->str);
}
