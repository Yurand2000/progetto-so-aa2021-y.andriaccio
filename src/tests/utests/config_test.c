#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "../source/errset.h"
#include "../source/config.h"

void test_00();
void test_01();
void test_02();

int main(int argc, const char* argv[])
{
	printf("CONFIG.C TEST START ==============\n");
	test_00();
	test_01();
	test_02();
	printf("CONFIG.C TEST END ================\n\n");
}

void test_00()
{
	printf("* * * start test 00\n");
	cfg_t config;
	if(read_config_file("./src/tests/utests/data/test_config_00.txt", &config) != 0)
	{
		printf("test00 fail\n");
		fflush(stdout);
		perror("read config file:");
		return;
	}

	printf("Read config file:\n");
	for(size_t i = 0; i < config.count; i++)
		printf("K:V = %s:%s\n", config.cfgs[i].key, config.cfgs[i].value);
	printf("test00 success!\n");
	free_config_file(&config);
}

void test_01()
{
	printf("* * * start test 01\n");
	cfg_t config;
	if(read_config_file("./src/tests/utests/data/test_config_01.txt", &config) != 0)
	{
		printf("test01 fail\n");
		fflush(stdout);
		perror("read config file:");
		return;
	}

	printf("Read config file:\n");
	printf("Key: config3; Value: %s\n", get_option(&config, "config3"));
	printf("Key: config5; Value: %s\n", get_option(&config, "config5"));
	printf("Key: config4; Value: %s\n", get_option(&config, "config4"));
	if(get_option(&config, "invalid_key") == NULL)
		printf("testing invalid key getter, success\n");
	else
		printf("testing invalid key getter, error\n");
	printf("test01 success!\n");
	free_config_file(&config);
}

void test_02()
{
	printf("* * * start test 02\n");
	cfg_t config;
	if(read_config_file("./src/tests/utests/data/test_config_02.txt", &config) != 0)
	{
		printf("test02 success, expected error Invalid Argument\n");
		fflush(stdout);
		perror("read config file:");
		return;
	}

	printf("Read config file:\n");
	for(size_t i = 0; i < config.count; i++)
		printf("K:V = %s:%s\n", config.cfgs[i].key, config.cfgs[i].value);
	printf("test02 fail!\n");
	free_config_file(&config);
}



