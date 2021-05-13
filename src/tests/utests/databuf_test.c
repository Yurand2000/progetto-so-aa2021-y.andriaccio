#include <stdio.h>
#include <stdlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "databuf.h"

void test_00();
void test_01();

int main(int argc, const char* argv[])
{
	test_00();
	test_01();
}

void test_00()
{
	databuf buf;
	int data1 = 15;
	int data2 = 987;
	const char str[] = "ciao mondo";

	create_data_buffer(&buf);
	//filling buffer
	write_buf(&buf, sizeof(int), &data1);
	write_buf(&buf, sizeof(int), &data2);
	write_buf(&buf, sizeof(str), str);

	assert(buf.buf_size == sizeof(int) * 2 + sizeof(str));

	//reading buffer
	int read1, read2;
	char read_buff[20];

	read_buf(&buf, sizeof(str), read_buff);
	read_buf(&buf, sizeof(int), &read2);
	read_buf(&buf, sizeof(int), &read1);
	
	assert(strncmp(str, read_buff, sizeof(str)) == 0);
	assert(read2 == data2);
	assert(read1 == data1);
	assert(buf.buf_size == 0);

	destroy_data_buffer(&buf);
	printf("test00 - success!\n");
}

void test_01()
{
	databuf buf;
	int err;

	err = create_data_buffer(&buf);
	assert(err == 0);

	int read1 = 1;
	long read2;
	err = read_buf(&buf, sizeof(int), &read1);
	assert(err == -1);
	perror("read buf expected error");

	err = write_buf(&buf, sizeof(int), &read1);
	assert(err == 0);

	err = read_buf(&buf, sizeof(long), &read2);
	assert(err == -1);
	perror("read buf expected error");

	err = destroy_data_buffer(&buf);
	assert(err == 0);

	printf("test01 - success!\n");
}




