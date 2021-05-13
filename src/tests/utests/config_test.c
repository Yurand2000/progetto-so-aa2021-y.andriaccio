#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "../source/errset.h"
#include "../source/config.h"

void test_00();
void test_01();

int main(int argc, const char* argv[])
{
	test_00();
	test_01();
}

void test_00()
{
	cfg_t config;
	if(read_config_file("test_config_00.txt", &config) != 0)
	{
		printf("test00 fail\n");
		fflush(stdout);
		perror("read config file:");
		return;
	}

	printf("Read config file:\n");
	for(size_t i = 0; i < config.count; i++)
		printf("K:V = %s:%s\n", config.cfgs[i].key, config.cfgs[i].value);
	printf("test00 success!");
	free_config_file(&config);
}

void test_01()
{

}




