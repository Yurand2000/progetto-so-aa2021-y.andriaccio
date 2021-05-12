#include "../source/string_utils.h"

#include <stdio.h>
#include <assert.h>
#include "../source/errset.h"

void test00();
void test01();

int main(int argc, const char* argv[])
{
	test00();
	test01();
}

void test00()
{
	string_t str1, str2;
	PERRCHECK((str_constr(&str1) == -1), "constructor error");
	PERRCHECK((str_destr(&str1) == -1), "destructor error");
	PERRCHECK((str_constr(NULL) != -1), "constructor 2 error");
	PERRCHECK((str_destr(NULL) != -1), "destructor 2 error");

	PERRCHECK((str_resize(&str1, 15) == -1), "resize error");
	assert(str1.cap == 15 && str1.len == 0);
	int check = 0;
	for(size_t i = 0; i < str1.len; i++)
		check = str1.str[i] != '\0';
	assert(check == 0);

	str_constr(&str2);
	PERRCHECK((str_reserve(&str2, 37) == -1), "reserve error");
	assert(str2.cap == 37 && str2.len == 0);
	assert(str_len(&str1) == 0 && str_capacity(&str2) == 37);
	assert(str_empty(&str1) && str_empty(&str2));

	PERRCHECK((str_assign_cstr(&str2, "string") == -1), "assign error");
	assert(str2.len == 6 && str2.cap == 37);
	assert(strcmp(str2.str, "string") == 0);

	PERRCHECK((str_assign(&str1, &str2) == -1), "assign 2 error");
	assert(str1.len == 6 && str1.cap == 15);
	assert(str_cmp(&str1, &str2) == 0);

	assert(*str_at_const(&str1, 3) == 'i');
	assert(*str_at(&str2, 2) == 'r');

	PERRCHECK((str_append_cstr(&str1, " string of hell") == -1), "append error");

	assert(str1.len == 21 && str1.cap == 21);
	PERRCHECK((str_append(&str2, &str1) == -1), "append 2 error");

	assert(str2.len == 27 && str2.cap == 37);

	str_resize(&str2, 27);
	assert(str2.cap == 27);

	str_resize(&str2, 5);
	assert(strcmp(str2.str, "strin") == 0);

	str_resize(&str2, 10);
	PERRCHECK((str_push_back(&str2, 'g') == -1), "push error");
	assert(str2.cap == 10);

	str_push_back(&str2, ' ');
	str_push_back(&str2, 't');
	str_push_back(&str2, 'e');
	str_push_back(&str2, 's');
	PERRCHECK((str_push_back(&str2, 't') == -1), "push error 2");
	assert(str2.cap >= 11);
	assert(strcmp(str_cstr(&str2), "string test") == 0);

	PERRCHECK((str_clear(&str2) == -1), "clear error");
	assert(str2.len == 0 && str2.cap >= 11);

	str_destr(&str1);
	str_destr(&str2);

	printf("test00 success\n");
}

void test01()
{

}
