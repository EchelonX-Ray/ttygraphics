#include <fcntl.h>
#include <sys/ioctl.h>
//#include <sys/mman.h>
#include <sys/wait.h>
#include <linux/kd.h>
#include <linux/fb.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>

#include "fb_setup.c"

#define MAX_ITERATIONS 1000
//#define MULTIPLIER 1
/*
//#define WIDTH 91
//#define HEIGHT 52
#define WIDTH 90
#define HEIGHT 50
#define SIZE WIDTH * HEIGHT
#define X_START WIDTH / -2
#define Y_START HEIGHT / -2
#define X_END WIDTH + X_START
#define Y_END HEIGHT + Y_START
*/

signed int fpid;
volatile signed int running;
volatile double v_fov;
sem_t mutex[4];
sem_t semtex[4];

struct com_section {
	unsigned int mutex_num;
	float start_x;
	float inc_x;
	float start_y;
	float inc_y;
	unsigned int x_length;
	unsigned int y_length;
	volatile uint32_t* bitmap;
	unsigned int bitmap_x;
	unsigned int bitmap_y;
};

void hfunc_SIGINT(int sig) {
	if (fpid > 0) {
		printf("\nSIGINT caught, relaying SIGINT to child.  Please wait...\n");
		if (kill(fpid, SIGINT)) {
			printf("Error: kill() failed to send SIGINT to child.  Parent is calling exit(11)\n");
			exit(12);
		}
		return;
	}
	if (running == 1) {
		running = 2;
	} else {
		exit(13);
	}
	return;
}

unsigned int compute_mandelbrot_point(register float real, register float imaginary, register unsigned int max_iterations) {
	register unsigned int i;
	register float tmp_real;
	register float tmp_imaginary;
	tmp_real = 0.0;
	tmp_imaginary = 0.0;
	i = 0;
	while (i < max_iterations) {
		register float tmp_f;
		tmp_f = ((tmp_real * tmp_real) - (tmp_imaginary * tmp_imaginary)) + real;
		tmp_imaginary = 2.0 * tmp_real * tmp_imaginary + imaginary;
		tmp_real = tmp_f;
		i++;
		if (isinf(tmp_real) || isinf(tmp_imaginary)) {
			return i;
		}
	}
	return 0;
}

void compute_mandelbrot_section(	float start_x, float inc_x, float start_y, float inc_y, 
											unsigned int x_length, unsigned int y_length, volatile uint32_t* bitmap, 
											unsigned int bitmap_x, unsigned int bitmap_y) {
	unsigned int iy;
	float fy;
	fy = start_y;
	iy = 0;
	while (iy < y_length) {
		unsigned int ix;
		float fx;
		fx = start_x;
		ix = 0;
		while (ix < x_length) {
			//register unsigned int color;
			//register unsigned int other_colors;
			register unsigned int result;
			//other_colors = 0;
			result = compute_mandelbrot_point(fx, fy, MAX_ITERATIONS);
			/*
			if (result == 0) {
				color = 0;
			} else {
				char red = 0xFF;
				char green = 0xFF;
				char blue = 0xFF;
				unsigned int tmp_val;
				if (result < MAX_ITERATIONS / 3) {
					tmp_val = (0xFF * 3 * result) / MAX_ITERATIONS;
					red = 0x00;
					green = 0x00;
					blue = tmp_val;
				}
				if (result < 2 * MAX_ITERATIONS / 3) {
					tmp_val = (0xFF * 3 * (result - 1 * MAX_ITERATIONS / 3)) / MAX_ITERATIONS;
					red = 0x00;
					green = tmp_val;
				} else {
					tmp_val = (0xFF * 3 * (result - 2 * MAX_ITERATIONS / 3)) / MAX_ITERATIONS;
					red = tmp_val;
				}
				color  = (unsigned int)red   << 16;
				color |= (unsigned int)green << 8;
				color |= (unsigned int)blue  << 0;
			}
			*/
			//color = 0x00FFFFFF * result / MAX_ITERATIONS;
			//color = 0x000000FF * result / MAX_ITERATIONS;
			/*
			if (result > MAX_ITERATIONS / 2) {
				result -= MAX_ITERATIONS / 2;
				other_colors = 0x1FE * result / MAX_ITERATIONS;
			}
			color |= (0xFF & other_colors) <<  8;
			color |= (0xFF & other_colors) << 16;
			*/
			bitmap[(iy + bitmap_y) * x_length + (ix + bitmap_x)] = result;
			//bitmap[(iy + bitmap_y) * x_length + (ix + bitmap_x)] = color;
			fx += inc_x;
			ix++;
		}
		fy += inc_y;
		iy++;
	}
}

void* sub_thread(void* param) {
	struct com_section* section;
	section = param;
	
	printf("Thread %d: [Starting]\n", section->mutex_num);
	//printf("Thread %d: start_y - %f\n", section->mutex_num, section->start_y);
	//printf("Thread %d: bitmap_y - %d\n", section->mutex_num, section->bitmap_y);
	while (running) {
		compute_mandelbrot_section(section->start_x, section->inc_x, section->start_y, section->inc_y, section->x_length, section->y_length, section->bitmap, section->bitmap_x, section->bitmap_y);
		sem_post(semtex + section->mutex_num);
		sem_wait(mutex + section->mutex_num);
		sem_wait(semtex + section->mutex_num);
	}
	sem_post(semtex + section->mutex_num);
	
	return NULL;
}

signed int main(signed int argc, char* argv[], char* envp[]) {
	
	/*
		Set the TTY to graphical mode and then fork() a child to actually use the graphical TTY.
		When the child then exits (or crashes), return the TTY to text mode and then exit.
		
		This allows recovery of the TTY in the event of a crash and is very useful for development.
		
		See the comment "Child Begin:" for where to begin child process code.
	*/
	
	running = 0;
	
	struct sigaction new_action;
	new_action.sa_handler = 0;
	sigemptyset(&(new_action.sa_mask));
	new_action.sa_flags = SA_NOCLDSTOP;
	struct sigaction old_action;
	
	if (sigaction(SIGCHLD, &new_action, &old_action) != 0) {
		printf("Error: SIGCHLD Action Flag Change Failed!\n");
		return 1;
	}
	
	signed int fd;
	signed int ret_val = 0;
	
	fd = 1;
	//fd = open("/dev/tty6", O_RDWR);
	if (fd < 0) {
		printf("Error: Invaild fd.  Possible open() failure!  Stage-1\n");
		return 4;
	}
	
	ret_val = 0;
	//ret_val = ioctl(fd, KDSETMODE, 1);
	if (ret_val != 0) {
		printf("Error: Failure Setting TTY Mode: To Graphical!\n");
		printf("errno: %s\n", strerror(errno));
		return 3;
	}
	//close(fd);
	
	fpid = fork();
	
	if (fpid > 0) {
		// Parent
		new_action.sa_handler = hfunc_SIGINT;
		sigemptyset(&(new_action.sa_mask));
		new_action.sa_flags = 0;
		if (sigaction(SIGINT, &new_action, 0) != 0) {
			printf("Error: SIGINT Handler Registration Failed! - Parent\n");
			return 8;
		}
		
		int fpid_ret_val;
		retry_waitpid:
		fpid_ret_val = 0;
		if (waitpid(fpid, &fpid_ret_val, 0) == -1) {
			if (errno == EINTR) {
				goto retry_waitpid;
			}
			printf("Error: waitpid() failed: %s\n", strerror(errno));
			return 11;
		}
		fpid_ret_val = WEXITSTATUS(fpid_ret_val);
		if (fpid_ret_val != 0) {
			printf("Error: Child exited with non-zero code: %d\nParent is continuing normal exit.\n", fpid_ret_val);
		}
		
		//fd = open("/dev/tty6", O_RDWR);
		if (fd < 0) {
			printf("Error: Invaild fd.  Possible open() failure!  Stage-2\n");
			return 4;
		}
		ret_val = 0;
		ret_val = ioctl(fd, KDSETMODE, 0);
		if (ret_val != 0) {
			printf("Error: Failure Setting TTY Mode: To Text!\n");
			printf("errno: %s\n", strerror(errno));
			return 3;
		}
		//close(fd);
		
		fprintf(stderr, "\x1b\x5b\x48\x1b\x5b\x4a\x1b\x5b\x33\x4a");
		fprintf(stdout, "\x1b\x5b\x48\x1b\x5b\x4a\x1b\x5b\x33\x4a");
		
		return 0;
	} else if (fpid == 0) {
		// Child
		if (sigaction(SIGCHLD, &old_action, 0) != 0) {
			printf("Error: SIGCHLD Action Restoration Failed!\n");
			return 1;
		}
	} else {
		// Fork Failed
		printf("Error: Fork Failed\n");
		
		//fd = open("/dev/tty6", O_RDWR);
		if (fd < 0) {
			printf("Error: Invaild fd.  Possible open() failure!  Stage-2\n");
			return 4;
		}
		ret_val = 0;
		ret_val = ioctl(fd, KDSETMODE, 0);
		if (ret_val != 0) {
			printf("Error: Failure Setting TTY Mode: To Text!\n");
			printf("errno: %s\n", strerror(errno));
			return 3;
		}
		//close(fd);
		
		return 2;
	}
	
	// Child Begin:
	char* drm_card_str = "/dev/dri/card1";
	char* fb_str = "/dev/fb0";
	struct con_fb* fbs = 0;
	
	fd = open(drm_card_str, O_RDWR | O_CLOEXEC);
	fprintf(stderr, "Trying to open Framebuffer with DRM and Double Buffering Enabled on %s ... \n", drm_card_str);
	if (fd >= 0) {
		fbs = try_drm_fb(fd, 1);
		if (fbs != 0) {
			goto successful_fbs_setup;
		}
		close(fd);
	}
	fprintf(stderr, " ... Failure\n");
	fd = open(fb_str, O_RDWR | O_CLOEXEC);
	fprintf(stderr, "Trying to open Framebuffer with Legacy Framebuffer and Double Buffering Workaround on %s ... \n", fb_str);
	if (fd >= 0) {
		fbs = try_legacy_fb(fd, 1);
		if (fbs != 0) {
			goto successful_fbs_setup;
		}
		close(fd);
	}
	fprintf(stderr, " ... Failure\n");
	fd = open(drm_card_str, O_RDWR | O_CLOEXEC);
	fprintf(stderr, "Trying to open Framebuffer with DRM and Double Buffering Disabled on %s ... \n", drm_card_str);
	if (fd >= 0) {
		fbs = try_drm_fb(fd, 0);
		if (fbs != 0) {
			goto successful_fbs_setup;
		}
		close(fd);
	}
	fprintf(stderr, " ... Failure\n");
	fd = open(fb_str, O_RDWR | O_CLOEXEC);
	fprintf(stderr, "Trying to open Framebuffer with Legacy Framebuffer without Double Buffering Workaround on %s ... \n", fb_str);
	if (fd >= 0) {
		fbs = try_legacy_fb(fd, 0);
		if (fbs != 0) {
			goto successful_fbs_setup;
		}
		close(fd);
	}
	fprintf(stderr, " ... Failure\n");
	fprintf(stderr, "Could not open Framebuffer!  Exiting.\n");
	return 1;
	
	successful_fbs_setup:
	fprintf(stderr, " ... Success\n");
	fprintf(stdout, "\nDrawing\n");
	
	unsigned int width;
	unsigned int height;
	/*
	unsigned int size;
	signed int x_start;
	signed int y_start;
	signed int x_end;
	signed int y_end;
	*/
	
	width = fbs[0].width;
	height = fbs[0].height;
	/*
	size = width * height;
	x_start = width / -2;
	y_start = height / -2;
	x_end = width + x_start;
	y_end = height + y_start;
	*/
	
	volatile uint32_t* bfb_ptr;
	uint32_t* bitmap;
	bitmap = malloc(width * height * sizeof(uint32_t));
	unsigned int x;
	unsigned int y;
	
	float x_start;
	float y_start;
	float x_inc;
	float y_inc;
	y_inc = 2.0 / height;
	x_inc = y_inc;
	y_start = -1.0;
	x_start = (-5.0 * width) / (3.5 * height);
	
	// Clear the screen
	y = 0;
	while (y < height) {
		x = 0;
		while (x < width) {
			bfb_ptr = fbs[0].fb_bfb + y * fbs[0].line_length + x * 4;
			*bfb_ptr = 0x00000000;
			x++;
		}
		y++;
	}
	
	swap_buffers(fd, fbs, 0);
	vsync_wait(fd, fbs, 0);
	
	running = 1;
	new_action.sa_handler = hfunc_SIGINT;
	sigemptyset(&(new_action.sa_mask));
	new_action.sa_flags = 0;
	if (sigaction(SIGINT, &new_action, 0) != 0) {
		printf("Error: SIGINT Handler Registration Failed! - Child\n");
		return 8;
	}
	
	v_fov = 2.0;
	struct com_section com_sec[4];
	pthread_t pthreads[4];
	x = 0;
	while (x < 4) {
		if(sem_init(mutex + x, 0, 1)) {
			printf("Error: Error creating mutex\n");
			return 9;
		}
		if(sem_init(semtex + x, 0, 1)) {
			printf("Error: Error creating semtex\n");
			return 13;
		}
		sem_wait(semtex + x);
		com_sec[x].mutex_num = x;
		com_sec[x].start_x = x_start;
		com_sec[x].start_y = y_start + (x * y_inc * height) / 4;
		com_sec[x].start_y = y_start + (x * y_inc * height) / 4;
		com_sec[x].start_y = y_start + (x * y_inc * height) / 4;
		com_sec[x].start_y = y_start + (x * y_inc * height) / 4;
		com_sec[x].inc_x = x_inc;
		com_sec[x].inc_y = y_inc;
		com_sec[x].x_length = width;
		com_sec[x].y_length = height / 4;
		com_sec[x].bitmap = bitmap;
		com_sec[x].bitmap_x = 0;
		com_sec[x].bitmap_y = (x * height) / 4;
		x++;
	}
	x = 0;
	while (x < 4) {
		if (pthread_create(pthreads + x, 0, sub_thread, com_sec + x)) {
			printf("Error: Error creating thread\n");
			return 10;
		}
		x++;
	}
	
	//compute_mandelbrot_section(x_start, x_inc, y_start, y_inc, width, height, bitmap, 0, 0);
	
	struct timespec req;
	req.tv_sec = 10;
	req.tv_nsec = 0;
	//nanosleep(&req, NULL);
	
	while (running) {
		x = 0;
		while (x < 4) {
			sem_wait(semtex + x);
			x++;
		}
		
		y = 0;
		while (y < height) {
			x = 0;
			while (x < width) {
				bfb_ptr = fbs[0].fb_bfb + y * fbs[0].line_length + x * 4;
				*bfb_ptr = bitmap[(height - (y + 1)) * width + x];
				x++;
			}
			y++;
		}
		
		printf("v_fov: %f\n", v_fov);
		x = 0;
		while (x < 4) {
			y_inc = v_fov / height;
			x_inc = y_inc;
			y_start = 0.0;
			x_start = -0.02;
			y_start += y_inc * height / -2.0;
			x_start += x_inc * width / -2.0;
			x_start += (-3.0 * width) / (height * 7.0);
			//x_start += x_inc * width * -2.5 / 3.5;
			
			com_sec[x].start_x = x_start;
			com_sec[x].start_y = y_start + (x * y_inc * height) / 4;
			com_sec[x].start_y = y_start + (x * y_inc * height) / 4;
			com_sec[x].start_y = y_start + (x * y_inc * height) / 4;
			com_sec[x].start_y = y_start + (x * y_inc * height) / 4;
			com_sec[x].inc_x = x_inc;
			com_sec[x].inc_y = y_inc;
			x++;
		}
		v_fov /= 2.0;
		
		if (running == 2) {
			running = 0;
		}
		
		x = 0;
		while (x < 4) {
			sem_post(semtex + x);
			x++;
		}
		x = 0;
		while (x < 4) {
			sem_post(mutex + x);
			x++;
		}
		
		swap_buffers(fd, fbs, 0);
		vsync_wait(fd, fbs, 0);
	}
	
	x = 0;
	while (x < 4) {
		printf("TraceA\n");
		pthread_join(pthreads[x], NULL);
		printf("TraceB\n");
		x++;
	}
	x = 0;
	while (x < 4) {
		sem_destroy(mutex + x);
		sem_destroy(semtex + x);
		x++;
	}
	
	req.tv_sec = 10;
	req.tv_nsec = 0;
	running = 1;
	while (running == 1 && 0) {
		nanosleep(&req, NULL);
	}
	
	free(bitmap);
	clean_up(fd, fbs);
	close(fd);
	
	return 0;
}
