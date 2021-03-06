#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
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

#include "fb_setup.c"

struct matrix {
	float x;
	float y;
	float z;
};
struct line {
	struct matrix p0;
	struct matrix p1;
	uint32_t p0_color;
	uint32_t p1_color;
};
struct cube {
	struct line lines[12];
};
struct camera {
	volatile struct matrix location;
	struct matrix looking_at;
	float h_fov;
	float v_fov;
	volatile unsigned int frame_counter;
};
struct slope {
	unsigned int type;
	float slope;
	float offset;
};

void hfunc_SIGINT(int sig); // SIGINT Signal Handler.  This is not registered until just before the pthreads are setup.
void* rotate_camera_func(void* ptr); // pThread Thread Function
void draw_grad_line(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, uint32_t color1, uint32_t color2, uint32_t* framebuffer, unsigned int fb_width);
uint32_t color_blend(uint32_t color1, uint32_t color2, unsigned char ratio);
unsigned int is_in_fov(struct camera* c, struct matrix* m, struct matrix* buffer);
void translate_rotation(struct matrix* point, struct matrix* rotation_point, struct matrix* rotation_delta, struct matrix* buffer);
float points_to_angle_2d(float x1, float y1, float x2, float y2);
void angle_to_points_2d(float angle, float magnitude, float x1, float y1, float* x2, float* y2);

volatile unsigned int running;
signed int fpid;
pthread_mutex_t mutex;

void hfunc_SIGINT(int sig) {
	if (fpid > 0) {
		printf("\nSIGINT caught, relaying SIGINT to child.  Please wait...\n");
		if (kill(fpid, SIGINT)) {
			printf("Error: kill() failed to send SIGINT to child.  Parent is calling exit(11)\n");
			exit(11);
		}
		return;
	}
	
	running = 0;
	return;
}

void* rotate_camera_func(void* ptr) {
	struct camera* cam = (struct camera*)ptr;
	
	unsigned int sec = 0;
	unsigned int fps = 0;
	while (running) {
		pthread_mutex_lock(&mutex);
		if(sec >= 100) {
			sec = 0;
			fps = cam->frame_counter;
			cam->frame_counter = 0;
			fprintf(stdout, "FPS: %d\n", fps);
		}
		float angle;
		float cam_x;
		float cam_z;
		cam_x = cam->location.x;
		cam_z = cam->location.z;
		angle = points_to_angle_2d(0.0, 0.0, cam_x, cam_z);
		angle += M_PI_2 / 90.0;
		angle_to_points_2d(angle, sqrt((cam->location.x * cam->location.x) + (cam->location.z * cam->location.z)), 0, 0, &cam_x, &cam_z);
		cam->location.x = cam_x;
		cam->location.z = cam_z;
		pthread_mutex_unlock(&mutex);
		usleep(10000);
		sec++;
	}
	
	return 0;
}

signed int main(signed int argc, char* argv[], char* envp[]) {
	
	/*
		Set the TTY to graphical mode and then fork() a child to actually use the graphical TTY.
		When the child then exits (or crashes), return the TTY to text mode and then exit.
		
		This allows recovery of the TTY in the event of a crash and is very useful for development.
		
		See the comment "Child Begin:" for where to begin child process code.
	*/
	
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
	ret_val = ioctl(fd, KDSETMODE, 1);
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
	char* drm_card_str = "/dev/dri/card0";
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
	
	struct cube box;
	struct camera cam;
	
	box.lines[ 0].p0.x = -1.0;
	box.lines[ 0].p0.y = +1.0;
	box.lines[ 0].p0.z = -1.0;
	box.lines[ 0].p0_color = 0x00FFFFFF;
	box.lines[ 0].p1.x = +1.0;
	box.lines[ 0].p1.y = +1.0;
	box.lines[ 0].p1.z = -1.0;
	box.lines[ 0].p1_color = 0x0000FFFF;
	
	box.lines[ 1].p0.x = +1.0;
	box.lines[ 1].p0.y = +1.0;
	box.lines[ 1].p0.z = -1.0;
	box.lines[ 1].p0_color = 0x0000FFFF;
	box.lines[ 1].p1.x = +1.0;
	box.lines[ 1].p1.y = -1.0;
	box.lines[ 1].p1.z = -1.0;
	box.lines[ 1].p1_color = 0x00FF00FF;
	
	box.lines[ 2].p0.x = +1.0;
	box.lines[ 2].p0.y = -1.0;
	box.lines[ 2].p0.z = -1.0;
	box.lines[ 2].p0_color = 0x00FF00FF;
	box.lines[ 2].p1.x = -1.0;
	box.lines[ 2].p1.y = -1.0;
	box.lines[ 2].p1.z = -1.0;
	box.lines[ 2].p1_color = 0x00FFFF00;
	
	box.lines[ 3].p0.x = -1.0;
	box.lines[ 3].p0.y = -1.0;
	box.lines[ 3].p0.z = -1.0;
	box.lines[ 3].p0_color = 0x00FFFF00;
	box.lines[ 3].p1.x = -1.0;
	box.lines[ 3].p1.y = +1.0;
	box.lines[ 3].p1.z = -1.0;
	box.lines[ 3].p1_color = 0x00FFFFFF;
	
	box.lines[ 4].p0.x = -1.0;
	box.lines[ 4].p0.y = +1.0;
	box.lines[ 4].p0.z = +1.0;
	box.lines[ 4].p0_color = 0x00FF0000;
	box.lines[ 4].p1.x = +1.0;
	box.lines[ 4].p1.y = +1.0;
	box.lines[ 4].p1.z = +1.0;
	box.lines[ 4].p1_color = 0x0000FF00;
	
	box.lines[ 5].p0.x = +1.0;
	box.lines[ 5].p0.y = +1.0;
	box.lines[ 5].p0.z = +1.0;
	box.lines[ 5].p0_color = 0x0000FF00;
	box.lines[ 5].p1.x = +1.0;
	box.lines[ 5].p1.y = -1.0;
	box.lines[ 5].p1.z = +1.0;
	box.lines[ 5].p1_color = 0x000000FF;
	
	box.lines[ 6].p0.x = +1.0;
	box.lines[ 6].p0.y = -1.0;
	box.lines[ 6].p0.z = +1.0;
	box.lines[ 6].p0_color = 0x000000FF;
	box.lines[ 6].p1.x = -1.0;
	box.lines[ 6].p1.y = -1.0;
	box.lines[ 6].p1.z = +1.0;
	box.lines[ 6].p1_color = 0x007F7F7F;
	
	box.lines[ 7].p0.x = -1.0;
	box.lines[ 7].p0.y = -1.0;
	box.lines[ 7].p0.z = +1.0;
	box.lines[ 7].p0_color = 0x007F7F7F;
	box.lines[ 7].p1.x = -1.0;
	box.lines[ 7].p1.y = +1.0;
	box.lines[ 7].p1.z = +1.0;
	box.lines[ 7].p1_color = 0x00FF0000;
	
	box.lines[ 8].p0.x = -1.0;
	box.lines[ 8].p0.y = +1.0;
	box.lines[ 8].p0.z = -1.0;
	box.lines[ 8].p0_color = 0x00FFFFFF;
	box.lines[ 8].p1.x = -1.0;
	box.lines[ 8].p1.y = +1.0;
	box.lines[ 8].p1.z = +1.0;
	box.lines[ 8].p1_color = 0x00FF0000;
	
	box.lines[ 9].p0.x = +1.0;
	box.lines[ 9].p0.y = +1.0;
	box.lines[ 9].p0.z = -1.0;
	box.lines[ 9].p0_color = 0x0000FFFF;
	box.lines[ 9].p1.x = +1.0;
	box.lines[ 9].p1.y = +1.0;
	box.lines[ 9].p1.z = +1.0;
	box.lines[ 9].p1_color = 0x0000FF00;
	
	box.lines[10].p0.x = +1.0;
	box.lines[10].p0.y = -1.0;
	box.lines[10].p0.z = -1.0;
	box.lines[10].p0_color = 0x00FF00FF;
	box.lines[10].p1.x = +1.0;
	box.lines[10].p1.y = -1.0;
	box.lines[10].p1.z = +1.0;
	box.lines[10].p1_color = 0x000000FF;
	
	box.lines[11].p0.x = -1.0;
	box.lines[11].p0.y = -1.0;
	box.lines[11].p0.z = -1.0;
	box.lines[11].p0_color = 0x00FFFF00;
	box.lines[11].p1.x = -1.0;
	box.lines[11].p1.y = -1.0;
	box.lines[11].p1.z = +1.0;
	box.lines[11].p1_color = 0x007F7F7F;
	
	cam.location.x   =   0.0;
	cam.location.y   =   0.0;
	cam.location.z   = -10.0;
	cam.looking_at.x =   0.0;
	cam.looking_at.y =   0.0;
	cam.looking_at.z =   0.0;
	
	float max_fov_degrees = 90.0;
	
	if (fbs[0].width > fbs[0].height) {
		cam.h_fov = max_fov_degrees;
		cam.v_fov = (max_fov_degrees * fbs[0].height) / fbs[0].width;
	} else {
		cam.v_fov = max_fov_degrees;
		cam.h_fov = (max_fov_degrees * fbs[0].width) / fbs[0].height;
	}
	
	// Create New Thread
	new_action.sa_handler = hfunc_SIGINT;
	sigemptyset(&(new_action.sa_mask));
	new_action.sa_flags = 0;
	if (sigaction(SIGINT, &new_action, 0) != 0) {
		printf("Error: SIGINT Handler Registration Failed! - Child\n");
		return 8;
	}
	
	cam.frame_counter = 0;
	running = 1;
	pthread_t rotate_camera_thread;
	if(pthread_mutex_init(&mutex, 0)) {
		printf("Error: Error creating mutex\n");
		return 9;
	}
	if(pthread_create(&rotate_camera_thread, 0, rotate_camera_func, &cam)) {
		printf("Error: Error creating thread\n");
		return 10;
	}
	
	//uint32_t* fptr = 0;
	//uint32_t* fbuffer = 0;
	//fptr = malloc(fbs[0].width * fbs[0].height * sizeof(uint32_t) * 2);
	//if (fptr == 0) {
	//	printf("Error: malloc() of fptr failed\n");
	//	exit(12);
	//}
	//fbuffer = fptr + fbs[0].width * fbs[0].height;
	
	struct matrix buffer_p0;
	struct matrix buffer_p1;
	unsigned int x_p0;
	unsigned int y_p0;
	unsigned int x_p1;
	unsigned int y_p1;
	unsigned int x_i;
	unsigned int y_i;
	unsigned int in_fov_p0;
	unsigned int in_fov_p1;
	volatile uint32_t* bfb_ptr;
	/*
	y_i = 0;
	while (y_i < fbs[0].height) {
		x_i = 0;
		while (x_i < fbs[0].width) {
			fptr[y_i * fbs[0].width + x_i + fbs[0].width * fbs[0].height] = 0xFF000000;
			x_i++;
		}
		y_i++;
	}
	*/
	while (running) {
		y_i = 0;
		while (y_i < fbs[0].height) {
			x_i = 0;
			while (x_i < fbs[0].width) {
				bfb_ptr = fbs[0].fb_bfb + y_i * fbs[0].line_length + x_i * 4;
				*bfb_ptr = 0x00000000;
				//fptr[y_i * fbs[0].width + x_i] = 0xFF000000;
				x_i++;
			}
			y_i++;
		}
		
		x_i = 0;
		while (x_i < 12) {
			pthread_mutex_lock(&mutex);
			in_fov_p0 = is_in_fov(&cam, &(box.lines[x_i].p0), &buffer_p0);
			in_fov_p1 = is_in_fov(&cam, &(box.lines[x_i].p1), &buffer_p1);
			pthread_mutex_unlock(&mutex);
			if (in_fov_p0) {
				x_p0 = buffer_p0.x / cam.h_fov * fbs[0].width;
				y_p0 = (cam.v_fov - buffer_p0.y) / cam.v_fov * fbs[0].height;
				bfb_ptr = fbs[0].fb_bfb + y_p0 * fbs[0].line_length + x_p0 * 4;
				*bfb_ptr = box.lines[x_i].p0_color;
				//fbuffer[y_p0 * fbs[0].width + x_p0] = box.lines[x_i].p0_color;
			}
			if (in_fov_p1) {
				x_p1 = buffer_p1.x / cam.h_fov * fbs[0].width;
				y_p1 = (cam.v_fov - buffer_p1.y) / cam.v_fov * fbs[0].height;
				bfb_ptr = fbs[0].fb_bfb + y_p1 * fbs[0].line_length + x_p1 * 4;
				*bfb_ptr = box.lines[x_i].p1_color;
				//fbuffer[y_p1 * fbs[0].width + x_p1] = box.lines[x_i].p1_color;
			}
			if (in_fov_p0 && in_fov_p1) {
				draw_grad_line(x_p0, y_p0, x_p1, y_p1, box.lines[x_i].p0_color, box.lines[x_i].p1_color, fbs[0].fb_bfb, fbs[0].line_length / 4);
			}
			x_i++;
		}
		/*
		y_i = 0;
		while (y_i < fbs[0].height) {
			x_i = 0;
			while (x_i < fbs[0].width) {
				if (fbuffer[y_i * fbs[0].width + x_i] == 0x00000000) {
					*((uint32_t*)(fbs[0].fb_bfb + x_i * 4 + y_i * fbs[0].line_length)) = 0x00000000;
					fbuffer[y_i * fbs[0].width + x_i] = 0x7F000000;
				} else {
					if (fbuffer[y_i * fbs[0].width + x_i] != 0xFF000000) {
						*((uint32_t*)(fbs[0].fb_bfb + x_i * 4 + y_i * fbs[0].line_length)) = fbuffer[y_i * fbs[0].width + x_i];
						fbuffer[y_i * fbs[0].width + x_i] = 0x00000000;
					}
				}
				x_i++;
			}
			y_i++;
		}
		*/
		swap_buffers(fd, fbs, 0);
		vsync_wait(fd, fbs, 0);
		cam.frame_counter++;
	}
	printf("Exiting\n");
	
	// Close and destroy threads
	running = 0;
	pthread_join(rotate_camera_thread, 0);
	pthread_mutex_destroy(&mutex);
	
	// Free up allocated memory
	//free(fptr);
	clean_up(fd, fbs);
	close(fd);
	
	return 0;
}
void draw_grad_line(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, uint32_t color1, uint32_t color2, uint32_t* framebuffer, unsigned int fb_width) {
	signed int xd;
	signed int yd;
	signed int xs;
	signed int ys;
	signed int yi;
	signed int x;
	signed int y;
	signed int i;
	if (x1 <= x2) {
		xs = x1;
		xd = x2 - x1;
		ys = y1;
		yi = 1;
	} else {
		xs = x2;
		xd = x1 - x2;
		ys = y2;
		yi = -1;
		color1 = color1 ^ color2;
		color2 = color1 ^ color2;
		color1 = color1 ^ color2;
	}
	if (y1 <= y2) {
		yd = y2 - y1;
	} else {
		yd = y1 - y2;
		yi *= -1;
	}
	i = 0;
	if (xd < yd) {
		while (i < yd) {
			x = (i * xd) / yd + xs;
			y = (i * yi) + ys;
			framebuffer[y * fb_width + x] = color_blend(color2, color1, i * 0xFF / yd);
			i++;
		}
	} else if (xd > yd) {
		while (i < xd) {
			x = i + xs;
			y = ((i * yi * yd) / xd) + ys;
			framebuffer[y * fb_width + x] = color_blend(color2, color1, i * 0xFF / xd);
			i++;
		}
	} else {
		while (i < xd) {
			x = i + xs;
			y = (i * yi) + ys;
			framebuffer[y * fb_width + x] = color_blend(color2, color1, i * 0xFF / xd);
			i++;
		}
	}
	return;
}
uint32_t color_blend(uint32_t color1, uint32_t color2, unsigned char ratio) {
	uint32_t result = 0;
	uint32_t tmpvar;
	uint32_t mask;
	mask = 0x000000FF;
	tmpvar = (((color1 & mask) * ratio) + ((color2 & mask) * (0xFF - ratio))) / 0xFF;
	result |= tmpvar & mask;
	mask = 0x0000FF00;
	tmpvar = (((color1 & mask) * ratio) + ((color2 & mask) * (0xFF - ratio))) / 0xFF;
	result |= tmpvar & mask;
	mask = 0x00FF0000;
	tmpvar = (((color1 & mask) * ratio) + ((color2 & mask) * (0xFF - ratio))) / 0xFF;
	result |= tmpvar & mask;
	mask = 0xFF000000;
	tmpvar = (((color1 & mask) * ratio) + ((color2 & mask) * (0xFF - ratio))) / 0xFF;
	result |= tmpvar & mask;
	return result;
}
unsigned int is_in_fov(struct camera* c, struct matrix* m, struct matrix *buffer) {
	// Translate point(struct matrix* m) to the camera angles
	struct matrix buffer2;
	struct matrix rotation_delta;
	struct matrix tmp_cam_loc;
	rotation_delta.z = 0.0;
	rotation_delta.y = points_to_angle_2d(c->location.x, c->location.z, c->looking_at.x, c->looking_at.z);
	if (c->looking_at.z - c->location.z < 0.0) {
		rotation_delta.x = points_to_angle_2d(c->location.y, c->looking_at.z, c->looking_at.y, c->location.z);
	} else {
		rotation_delta.x = points_to_angle_2d(c->location.y, c->location.z, c->looking_at.y, c->looking_at.z);
	}
	tmp_cam_loc.x = c->location.x;
	tmp_cam_loc.y = c->location.y;
	tmp_cam_loc.z = c->location.z;
	translate_rotation(m, &tmp_cam_loc, &rotation_delta, &buffer2);
	
	// Find the max and min slopes and offsets, per the equation [y = mx + b], for the camera's FOV and location.
	// The camera is assumed to be pointing straight forward with no angles since the point is translated to 
	// match it above instead.  4 relationships are computed: A min and max corresponding to the camera's FOV 
	// along the x/z and y/z axis.
	
	// Variable Nameing Convention:
	//   Minimums are prefixed with l_ and Maximums are prefixed with h_
	//   Association of the Z-Axis as the x varable in the equation [y = mx + b] is assumed.
	//   The value with an association to the y varable in the equation [y = mx + b], is specifed by the value after l_ or h_.
	//   Whether the value is associated with m or b is indicated by weather it is suffixed with a slope or offset, respectively.
	float h_xslope;
	float h_yslope;
	float l_xslope;
	float l_yslope;
	float h_xoffset;
	float h_yoffset;
	float l_xoffset;
	float l_yoffset;
	h_xslope = tanf((c->h_fov * M_PI) / 360.0);
	h_yslope = tanf((c->v_fov * M_PI) / 360.0);
	l_xslope = -h_xslope;
	l_yslope = -h_yslope;
	h_xoffset = c->location.x - (h_xslope * c->location.z);
	h_yoffset = c->location.y - (h_yslope * c->location.z);
	l_xoffset = c->location.x - (l_xslope * c->location.z);
	l_yoffset = c->location.y - (l_yslope * c->location.z);
	
	// Plug the values into the 4 computed equations and determine whether the point is outside the camera's FOV.
	// If it is, return 0.
	if (buffer2.x > (h_xslope * buffer2.z) + h_xoffset) {
		return 0;
	}
	if (buffer2.x < (l_xslope * buffer2.z) + l_xoffset) {
		return 0;
	}
	if (buffer2.y > (h_yslope * buffer2.z) + h_yoffset) {
		return 0;
	}
	if (buffer2.y < (l_yslope * buffer2.z) + l_yoffset) {
		return 0;
	}
	
	// If we have made it this far, we are in the camera's FOV.  So we should:
	//   -Store in the return buffer, the proportion of the location of the point, to the camera's FOV, for the x and y axis.
	//   --Translate as necessary such that the minimum X and Y FOVs start at 0 and not at -0.5 * (FOV angle).
	//   -Return 1.
	float x;
	float y;
	float z;
	x = points_to_angle_2d(c->location.x, c->location.z, buffer2.x, buffer2.z) * 180 / M_PI + (c->h_fov / 2);
	y = points_to_angle_2d(c->location.y, c->location.z, buffer2.y, buffer2.z) * 180 / M_PI + (c->v_fov / 2);
	z = 0.0; // This value is not used.
	buffer->x = x;
	buffer->y = y;
	buffer->z = z;
	return 1;
}
void translate_rotation(struct matrix* target_point, struct matrix* rotation_point, struct matrix* rotation_delta, struct matrix* buffer) {
	float delta_x = target_point->x - rotation_point->x;
	float delta_y = target_point->y - rotation_point->y;
	float delta_z = target_point->z - rotation_point->z;
	float angle;
	buffer->x = 0.0;
	buffer->y = 0.0;
	buffer->z = 0.0;
	
	// Translate Around Y-Axis
	angle = points_to_angle_2d(rotation_point->x, rotation_point->z, target_point->x, target_point->z);
	angle -= rotation_delta->y;
	angle_to_points_2d(angle, sqrtf( (delta_x * delta_x) + \
												(delta_z * delta_z) ), \
												rotation_point->x, rotation_point->z, &(buffer->x), &(buffer->z));
	
	// Translate Around X-Axis
	angle = points_to_angle_2d(rotation_point->y, rotation_point->z, target_point->y, buffer->z);
	angle -= rotation_delta->x;
	angle_to_points_2d(angle, sqrtf(	(delta_y * delta_y) + \
												((buffer->z - rotation_point->z) * (buffer->z - rotation_point->z)) ), \
												rotation_point->y, rotation_point->z, &(buffer->y), &(buffer->z));
	
	return;
}
float points_to_angle_2d(float x1, float y1, float x2, float y2) {
	x2 -= x1;
	y2 -= y1;
	if (x2 == 0) {
		x1 = 0; // atanf(0) == 0
	} else {
		x1 = atanf(x2 / y2);
	}
	if (y2 < 0) {
		x1 += M_PI;
	}
	if (x1 > M_PI) {
		x1 -= M_PI * 2;
	}
	return x1;
}
void angle_to_points_2d(float angle, float magnitude, float x1, float y1, float* x2, float* y2) {
	*x2 = x1 + sinf(angle) * magnitude;
	*y2 = y1 + cosf(angle) * magnitude;
	return;
}
