#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

signed int main(signed int argc, char* argv[], char* envp[]) {
	signed int fd;
	unsigned long request;
	signed int ret_val;
	signed int errno_bak;
	
	if (argc < 2) {
		fprintf(stderr, "Error: Too Few Arguments.  Expected the path to a broken tty in argv[1].\n");
		fprintf(stderr, "Example Usage: ./fix_my_tty.out /dev/tty6\n");
		return 1;
	}
	
	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		errno_bak = errno;
		fprintf(stderr, "Error: Invaild fd.  Possible open() failure!\n");
		fprintf(stderr, "errno: %s\n", strerror(errno_bak));
		return 2;
	}
	
	ret_val = ioctl(fd, KDSETMODE, 0);
	if (ret_val != 0) {
		errno_bak = errno;
		fprintf(stderr, "Error: Failure Setting TTY Mode: To Graphical!\n");
		fprintf(stderr, "errno: %s\n", strerror(errno_bak));
		return 3;
	}
	
	ret_val = close(fd);
	if (ret_val != 0) {
		errno_bak = errno;
		fprintf(stderr, "Error: close(fd) failed.\n");
		fprintf(stderr, "errno: %s\n", strerror(errno_bak));
		return 4;
	}
	
	return 0;
}
