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
	
	/*
		Set the TTY to graphical mode and then fork() a child to actually use the graphical TTY.
		When the child then exits (or crashes), return the TTY to text mode and then exit.
		
		This allows recovery of the TTY in the event of a crash and is very useful for development.
		
		See the comment "Child Begin:" for where to begin child process code.
	*/
	
	struct sigaction new_action;
	new_action.sa_handler = 0;
	new_action.sa_sigaction = 0;
	//sigemptyset(&(new_action.sa_mask));
	new_action.sa_flags = SA_NOCLDSTOP;
	new_action.sa_restorer = 0;
	struct sigaction old_action;
	/*
	if (sigaction(SIGCHLD, &new_action, &old_action) != 0) {
		printf("SIGCHLD Action Flag Change Failed!\n");
		return 1;
	}
	*/
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
	
	signed int fpid = 0;
	fpid = fork();

	if (fpid > 0) {
		// Parent
		wait(0);
		
		//fd = open("/dev/tty6", O_RDWR);
		if (fd < 0) {
			printf("Invaild fd.  Possible open() failure!  Stage-2\n");
			return 4;
		}
		ret_val = ioctl(fd, KDSETMODE, 0);
		if (ret_val != 0) {
			printf("Failure Setting TTY Mode: To Text!\n");
			printf("errno: %s\n", strerror(errno));
			return 3;
		}
		//close(fd);
		
		return 0;
	} else if (fpid == 0) {
		// Child
		if (sigaction(SIGCHLD, &old_action, 0) != 0) {
			printf("SIGCHLD Action Restoration Failed!\n");
			return 1;
		}
	} else {
		// Fork Failed
		printf("Fork Failed\n");
		return 2;
	}
	
	// Child Begin:
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	
	fd = open("/dev/fb0", O_RDWR);
	if (fd < 0) {
		printf("Invaild fd.  Possible open() failure!  Stage-3\n");
		return 4;
	}
	
	if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) != 0) {
		printf("Error reading fixed information\n");
		return 5;
	}
	if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) != 0) {
		printf("Error reading variable information\n");
		return 5;
	}
	
	printf("Vinfo: Res - %dx%d, BPP - %d\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);
	
	unsigned char* mbyte;
	void* ptr = 0;
	unsigned int screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
	
	ptr = mmap(ptr, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (ptr == MAP_FAILED || ptr == 0) {
		printf("Error: mmap() Failed!  Could not create virtual address space mapping!\n");
		printf("errno: %s\n", strerror(errno));
		return 6;
	}
	mbyte = ptr;
	
	unsigned int x = 0;
	unsigned int y = 0;
	unsigned int i = 0;
	y = 0;
	while (y < vinfo.yres) {
		x = 0;
		while (x < vinfo.xres) {
			i = 0;
			while (i < (vinfo.bits_per_pixel / 8)) {
				printf("TraceA\n");
				mbyte[(((y * vinfo.xres) + x) * (vinfo.bits_per_pixel / 8)) + i] = 0x00;
				printf("TraceB\n");
				i++;
			}
			x++;
		}
		y++;
	}
	
	munmap(ptr, screensize);
	
	close(fd);
	
	printf("Finished!\n");
	
	return 0;
}
