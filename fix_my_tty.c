#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <linux/kd.h>
#include <linux/fb.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

signed int main(signed int argc, char* argv[], char* envp[]) {	
	struct sigaction new_action;
	new_action.sa_handler = 0;
	new_action.sa_sigaction = 0;
	new_action.sa_flags = SA_NOCLDSTOP;
	new_action.sa_restorer = 0;
	struct sigaction old_action;
	
	signed int fd;
	unsigned long request;
	signed int ret_val;
	
	//fd = 1;
	fd = open("/dev/tty6", O_RDWR);
	if (fd < 0) {
		printf("Invaild fd.  Possible open() failure!  Stage-1\n");
		return 4;
	}
	
	ret_val = ioctl(fd, KDSETMODE, 0);
	if (ret_val != 0) {
		printf("Failure Setting TTY Mode: To Graphical!\n");
		printf("errno: %s\n", strerror(errno));
		return 3;
	}
	close(fd);
	
	return 0;
}
