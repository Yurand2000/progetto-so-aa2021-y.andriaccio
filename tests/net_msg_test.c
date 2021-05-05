#include <stdio.h>
#include <stdlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../source/net_msg.h"

void test_00();
void test_01();

int main(int argc, const char* argv[])
{
	test_00();
	test_01();
}

void test_00()
{
	net_msg msg, readmsg;
	create_message(&msg);

	msg.type = 0x0001;
	int data = 0x72727272;
	write_buf(&msg.data, sizeof(int), &data);

	set_checksum(&msg);
	printf("Generated checksum: %lu\n", msg.checksum);

	//printing to stdout the raw message
	printf("Message raw on stdout: ");
	fflush(stdout);
	write_msg(1, &msg);
	printf("\n");

	int temp_file;
	unlink("./temp");
	if((temp_file = open("./temp", O_CREAT | O_RDWR, 666)) == -1)
	{
		perror("open error");
		return;
	}

	write_msg(temp_file, &msg);
	lseek(temp_file, 0, SEEK_SET);
	printf("written message to temporary file\n");

	create_message(&readmsg);

	assert(read_msg(temp_file, &readmsg) == 0);
	assert(check_checksum(&readmsg) == 0);
	
	printf("Message raw on stdout: ");
	fflush(stdout);
	write_msg(1, &readmsg);
	printf("\n");

	assert(msg.type == readmsg.type);
	assert(msg.checksum == readmsg.checksum);

	destroy_message(&msg);
	destroy_message(&readmsg);

	close(temp_file);
	unlink("./temp");

	printf("test 00 success!\n");
}

void test_01()
{

}
