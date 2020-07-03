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

struct matrix {
	float x;
	float y;
	float z;
};
struct line {
	struct matrix p0;
	struct matrix p1;
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
unsigned int DEBUG_is_in_fov(struct camera* c, struct matrix* m, struct matrix* buffer);
unsigned int is_in_fov(struct camera* c, struct matrix* m, struct matrix* buffer);
void DEBUG_translate_rotation(struct matrix* point, struct matrix* rotation_point, struct matrix* rotation_delta, struct matrix* buffer);
void translate_rotation(struct matrix* point, struct matrix* rotation_point, struct matrix* rotation_delta, struct matrix* buffer);
float DEBUG_points_to_angle_2d(float x1, float y1, float x2, float y2);
float points_to_angle_2d(float x1, float y1, float x2, float y2);
void angle_to_points_2d(float angle, float magnitude, float x1, float y1, float* x2, float* y2);
//unsigned int greater_than_point(struct matrix* po, struct matrix* p1, struct matrix* p2);
//unsigned int less_than_point(struct matrix* po, struct matrix* p1, struct matrix* p2);
void add_m(struct matrix* m1, struct matrix* m2, struct matrix* mr);
void sub_m(struct matrix* m1, struct matrix* m2, struct matrix* mr);
//void mul_m(struct matrix* m1, struct matrix* m2, struct matrix* mr);
//void div_m(struct matrix* m1, struct matrix* m2, struct matrix* mr);

volatile unsigned int running;
signed int fpid;
pthread_mutex_t mutex;

void hfunc_SIGINT(int sig) {
	if (fpid > 0) {
		printf("\nSIGINT caught, relaying SIGINT to child.  Please wait.\n");
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
			printf("FPS: %d\n", fps);
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
		//printf("angle: %11.6f, cam.x: %11.6f, cam.y: %11.6f, cam.z: %11.6f\n", angle * 180 / M_PI, cam_x, cam->location.y, cam_z);
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
		//ret_val = ioctl(fd, KDSETMODE, 0);
		if (ret_val != 0) {
			printf("Error: Failure Setting TTY Mode: To Text!\n");
			printf("errno: %s\n", strerror(errno));
			return 3;
		}
		//close(fd);
		
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
		return 2;
	}
	
	// Child Begin:
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	
	fd = open("/dev/fb0", O_RDWR);
	if (fd < 0) {
		printf("Error: Invaild fd.  Possible open() failure!  Stage-3\n");
		return 4;
	}
	
	if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) != 0) {
		printf("Error: Error reading fixed information\n");
		return 5;
	}
	if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) != 0) {
		printf("Error: Error reading variable information\n");
		return 5;
	}
	
	printf("Vinfo: Res - %dx%d, BPP - %d\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);
	
	if (vinfo.bits_per_pixel != 32) {
		printf("Error: Wrong bits per pixel!\n");
		return 6;
	}
	
	uint32_t* mcolor;
	void* ptr = 0;
	
	errno = 0;
	ptr = mmap(ptr, vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (ptr == MAP_FAILED || ptr == 0 || errno != 0) {
		printf("Error: mmap() Failed!  Could not create virtual address space mapping!\n");
		printf("errno: %s\n", strerror(errno));
		return 7;
	}
	mcolor = ptr;
	
	struct cube box;
	struct camera cam;
	
	box.lines[ 0].p0.x = -1.0;
	box.lines[ 0].p0.y = +1.0;
	box.lines[ 0].p0.z = -1.0;
	box.lines[ 0].p1.x = +1.0;
	box.lines[ 0].p1.y = +1.0;
	box.lines[ 0].p1.z = -1.0;
	
	box.lines[ 1].p0.x = +1.0;
	box.lines[ 1].p0.y = +1.0;
	box.lines[ 1].p0.z = -1.0;
	box.lines[ 1].p1.x = +1.0;
	box.lines[ 1].p1.y = -1.0;
	box.lines[ 1].p1.z = -1.0;
	
	box.lines[ 2].p0.x = +1.0;
	box.lines[ 2].p0.y = -1.0;
	box.lines[ 2].p0.z = -1.0;
	box.lines[ 2].p1.x = -1.0;
	box.lines[ 2].p1.y = -1.0;
	box.lines[ 2].p1.z = -1.0;
	
	box.lines[ 3].p0.x = -1.0;
	box.lines[ 3].p0.y = -1.0;
	box.lines[ 3].p0.z = -1.0;
	box.lines[ 3].p1.x = -1.0;
	box.lines[ 3].p1.y = +1.0;
	box.lines[ 3].p1.z = -1.0;
	
	box.lines[ 4].p0.x = -1.0;
	box.lines[ 4].p0.y = +1.0;
	box.lines[ 4].p0.z = +1.0;
	box.lines[ 4].p1.x = +1.0;
	box.lines[ 4].p1.y = +1.0;
	box.lines[ 4].p1.z = +1.0;
	
	box.lines[ 5].p0.x = +1.0;
	box.lines[ 5].p0.y = +1.0;
	box.lines[ 5].p0.z = +1.0;
	box.lines[ 5].p1.x = +1.0;
	box.lines[ 5].p1.y = -1.0;
	box.lines[ 5].p1.z = +1.0;
	
	box.lines[ 6].p0.x = +1.0;
	box.lines[ 6].p0.y = -1.0;
	box.lines[ 6].p0.z = +1.0;
	box.lines[ 6].p1.x = -1.0;
	box.lines[ 6].p1.y = -1.0;
	box.lines[ 6].p1.z = +1.0;
	
	box.lines[ 7].p0.x = -1.0;
	box.lines[ 7].p0.y = -1.0;
	box.lines[ 7].p0.z = +1.0;
	box.lines[ 7].p1.x = -1.0;
	box.lines[ 7].p1.y = +1.0;
	box.lines[ 7].p1.z = +1.0;
	
	/*
	box.lines[ 8].p0.x = -1.0;
	box.lines[ 8].p0.y = +1.0;
	box.lines[ 8].p0.z = -1.0;
	box.lines[ 8].p1.x = -1.0;
	box.lines[ 8].p1.y = +1.0;
	box.lines[ 8].p1.z = +1.0;
	
	box.lines[ 9].p0.x = +1.0;
	box.lines[ 9].p0.y = +1.0;
	box.lines[ 9].p0.z = -1.0;
	box.lines[ 9].p1.x = +1.0;
	box.lines[ 9].p1.y = +1.0;
	box.lines[ 9].p1.z = +1.0;
	
	box.lines[10].p0.x = +1.0;
	box.lines[10].p0.y = -1.0;
	box.lines[10].p0.z = -1.0;
	box.lines[10].p1.x = +1.0;
	box.lines[10].p1.y = -1.0;
	box.lines[10].p1.z = +1.0;
	
	box.lines[11].p0.x = -1.0;
	box.lines[11].p0.y = -1.0;
	box.lines[11].p0.z = -1.0;
	box.lines[11].p1.x = -1.0;
	box.lines[11].p1.y = -1.0;
	box.lines[11].p1.z = +1.0;
	*/
	
	uint32_t pt_colors[8];
	pt_colors[ 0] = 0x00FFFFFF;
	pt_colors[ 1] = 0x0000FFFF;
	pt_colors[ 2] = 0x00FF00FF;
	pt_colors[ 3] = 0x00FFFF00;
	pt_colors[ 4] = 0x000000FF;
	pt_colors[ 5] = 0x0000FF00;
	pt_colors[ 6] = 0x00FF0000;
	pt_colors[ 7] = 0x007F7F7F;
	
	cam.location.x =  10.0;
	cam.location.y =   0.0;
	cam.location.z =   0.0;
	cam.looking_at.x =   0.0;
	cam.looking_at.y =   0.0;
	cam.looking_at.z =   0.0;
	
	float max_fov_degrees = 90.0;
	
	if (vinfo.xres > vinfo.yres) {
		cam.h_fov = max_fov_degrees;
		cam.v_fov = (max_fov_degrees * vinfo.yres) / vinfo.xres;
	} else {
		cam.v_fov = max_fov_degrees;
		cam.h_fov = (max_fov_degrees * vinfo.xres) / vinfo.yres;
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
	
	unsigned int old_points[8];
	old_points[0] = 0;
	old_points[1] = 0;
	old_points[2] = 0;
	old_points[3] = 0;
	old_points[4] = 0;
	old_points[5] = 0;
	old_points[6] = 0;
	old_points[7] = 0;
	struct matrix buffer;
	uint32_t color;
	unsigned int i;
	unsigned int x;
	unsigned int y;
	unsigned int in_fov;
	while (running) {
		i = 0;
		while (i < 8) {
			pthread_mutex_lock(&mutex);
			in_fov = is_in_fov(&cam, &(box.lines[i].p0), &buffer);
			if (in_fov == 0) {
				printf("i: %d\n", i);
				DEBUG_is_in_fov(&cam, &(box.lines[i].p0), &buffer);
			}
			pthread_mutex_unlock(&mutex);
			if (in_fov) {
				color = pt_colors[i];
				x = buffer.x / cam.h_fov * vinfo.xres;
				y = buffer.y / cam.v_fov * vinfo.yres;
				mcolor[old_points[i]] = 0x00000000;
				old_points[i] = y * vinfo.xres + x;
				mcolor[old_points[i]] = color;
			} else {
				printf("Failure: Out of camera FOV.\n");
			}
			i++;
		}
		cam.frame_counter++;
	}
	
	// Close and destroy threads
	running = 0;
	pthread_join(rotate_camera_thread, 0);
	pthread_mutex_destroy(&mutex);
	
	munmap(ptr, vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8);
	
	close(fd);
	
	return 0;
}
unsigned int DEBUG_is_in_fov(struct camera* c, struct matrix* m, struct matrix *buffer) {
	// Translate point(struct matrix* m) to the camera angles
	struct matrix buffer2;
	struct matrix rotation_delta;
	struct matrix tmp_cam_loc;
	rotation_delta.z = 0.0;
	rotation_delta.y = DEBUG_points_to_angle_2d(c->location.x, c->location.z, c->looking_at.x, c->looking_at.z);
	if (c->looking_at.z - c->location.z < 0.0) {
		rotation_delta.x = DEBUG_points_to_angle_2d(c->location.y, c->looking_at.z, c->looking_at.y, c->location.z);
	} else {
		rotation_delta.x = DEBUG_points_to_angle_2d(c->location.y, c->location.z, c->looking_at.y, c->looking_at.z);
	}
	printf("rotd_x: %11.6f, rotd_y: %11.6f\n", rotation_delta.x * 180 / M_PI, rotation_delta.y * 180 / M_PI);
	tmp_cam_loc.x = c->location.x;
	tmp_cam_loc.y = c->location.y;
	tmp_cam_loc.z = c->location.z;
	printf("point.x: %11.6f, point.y: %11.6f, point.z: %11.6f\n", m->x, m->y, m->z);
	DEBUG_translate_rotation(m, &tmp_cam_loc, &rotation_delta, &buffer2);
	printf("camera.looking_at.x: %11.6f, camera.looking_at.y: %11.6f, camera.looking_at.z: %11.6f\n", c->looking_at.x, c->looking_at.y, c->looking_at.z);
	printf("camera.location.x: %11.6f, camera.location.y: %11.6f, camera.location.z: %11.6f\n", c->location.x, c->location.y, c->location.z);
	printf("buffer2.x: %11.6f, buffer2.y: %11.6f, buffer2.z: %11.6f\n", buffer2.x, buffer2.y, buffer2.z);
	//printf("cx: %11.6f, cy: %11.6f, cz: %11.6f, bx: %11.6f, by: %11.6f, bz: %11.6f\n", c->location.x, c->location.y, c->location.z, buffer2.x, buffer2.y, buffer2.z);
	//exit(0);
	
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
		printf("TraceA\n");
		exit(50);
		return 0;
	}
	if (buffer2.x < (l_xslope * buffer2.z) + l_xoffset) {
		printf("TraceB\n");
		exit(50);
		return 0;
	}
	if (buffer2.y > (h_yslope * buffer2.z) + h_yoffset) {
		printf("TraceC\n");
		exit(50);
		return 0;
	}
	if (buffer2.y < (l_yslope * buffer2.z) + l_yoffset) {
		printf("TraceD\n");
		exit(50);
		return 0;
	}
	
	// If we have made it this far, we are in the camera's FOV.  So we should:
	//   -Store in the return buffer, the proportion of the location of the point, to the camera's FOV, for the x and y axis.
	//   --Translate as necessary such that the minimum X and Y FOVs start at 0 and not at -0.5 * (FOV angle).
	//   -Return 1.
	float x;
	float y;
	float z;
	//printf("cam.x: %f, cam.y: %f, cam.z: %f\n", c->location.x, c->location.y, c->location.z);
	//printf("buffer2.x: %f, buffer2.y: %f, buffer2.z: %f\n", buffer2.x, buffer2.y, buffer2.z);
	x = DEBUG_points_to_angle_2d(c->location.x, c->location.z, buffer2.x, buffer2.z) * 180 / M_PI + (c->h_fov / 2);
	y = DEBUG_points_to_angle_2d(c->location.y, c->location.z, buffer2.y, buffer2.z) * 180 / M_PI + (c->v_fov / 2);
	z = 0.0; // This value is not used.
	buffer->x = x;
	buffer->y = y;
	buffer->z = z;
	return 1;
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
	//printf("rotd_x: %11.6f, rotd_y: %11.6f\n", rotation_delta.x * 180 / M_PI, rotation_delta.y * 180 / M_PI);
	tmp_cam_loc.x = c->location.x;
	tmp_cam_loc.y = c->location.y;
	tmp_cam_loc.z = c->location.z;
	translate_rotation(m, &tmp_cam_loc, &rotation_delta, &buffer2);
	//printf("cx: %11.6f, cy: %11.6f, cz: %11.6f, bx: %11.6f, by: %11.6f, bz: %11.6f\n", c->location.x, c->location.y, c->location.z, buffer2.x, buffer2.y, buffer2.z);
	//exit(0);
	
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
		//printf("TraceA\n");
		return 0;
	}
	if (buffer2.x < (l_xslope * buffer2.z) + l_xoffset) {
		//printf("TraceB\n");
		//exit(50);
		return 0;
	}
	if (buffer2.y > (h_yslope * buffer2.z) + h_yoffset) {
		//printf("TraceC\n");
		return 0;
	}
	if (buffer2.y < (l_yslope * buffer2.z) + l_yoffset) {
		//printf("TraceD\n");
		return 0;
	}
	
	// If we have made it this far, we are in the camera's FOV.  So we should:
	//   -Store in the return buffer, the proportion of the location of the point, to the camera's FOV, for the x and y axis.
	//   --Translate as necessary such that the minimum X and Y FOVs start at 0 and not at -0.5 * (FOV angle).
	//   -Return 1.
	float x;
	float y;
	float z;
	//printf("cam.x: %f, cam.y: %f, cam.z: %f\n", c->location.x, c->location.y, c->location.z);
	//printf("buffer2.x: %f, buffer2.y: %f, buffer2.z: %f\n", buffer2.x, buffer2.y, buffer2.z);
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
	//printf("(Stage1) angle1: %f\n", angle * 180 / M_PI);
	angle -= rotation_delta->y;
	//printf("(Stage1) angle2: %f\n", angle * 180 / M_PI);
	angle_to_points_2d(angle, sqrtf( (delta_x * delta_x) + \
												(delta_z * delta_z) ), \
												rotation_point->x, rotation_point->z, &(buffer->x), &(buffer->z));
	//printf("(Stage1) buffer->x: %11.6f, buffer->y: %11.6f, buffer->z: %11.6f\n", buffer->x, buffer->y, buffer->z);
	
	// Translate Around X-Axis
	angle = points_to_angle_2d(rotation_point->y, rotation_point->z, target_point->y, buffer->z);
	//printf("(Stage2) angle1: %f\n", angle * 180 / M_PI);
	angle -= rotation_delta->x;
	//printf("(Stage2) angle2: %f\n", angle * 180 / M_PI);
	angle_to_points_2d(angle, sqrtf(	(delta_y * delta_y) + \
												((buffer->z - rotation_point->z) * (buffer->z - rotation_point->z)) ), \
												rotation_point->y, rotation_point->z, &(buffer->y), &(buffer->z));
	//printf("(Stage2) buffer->x: %11.6f, buffer->y: %11.6f, buffer->z: %11.6f\n", buffer->x, buffer->y, buffer->z);
	
	return;
}
void DEBUG_translate_rotation(struct matrix* target_point, struct matrix* rotation_point, struct matrix* rotation_delta, struct matrix* buffer) {
	float delta_x = target_point->x - rotation_point->x;
	float delta_y = target_point->y - rotation_point->y;
	float delta_z = target_point->z - rotation_point->z;
	float angle;
	buffer->x = 0.0;
	buffer->y = 0.0;
	buffer->z = 0.0;
	
	// Translate Around Y-Axis
	angle = points_to_angle_2d(rotation_point->x, rotation_point->z, target_point->x, target_point->z);
	printf("(Stage1) angle1: %f\n", angle * 180 / M_PI);
	angle -= rotation_delta->y;
	printf("(Stage1) angle2: %f\n", angle * 180 / M_PI);
	angle_to_points_2d(angle, sqrtf( (delta_x * delta_x) + \
												(delta_z * delta_z) ), \
												rotation_point->x, rotation_point->z, &(buffer->x), &(buffer->z));
	printf("(Stage1) buffer->x: %11.6f, buffer->y: %11.6f, buffer->z: %11.6f\n", buffer->x, buffer->y, buffer->z);
	
	// Translate Around X-Axis
	angle = points_to_angle_2d(rotation_point->y, rotation_point->z, target_point->y, buffer->z);
	printf("(Stage2) angle1: %f\n", angle * 180 / M_PI);
	angle -= rotation_delta->x;
	printf("(Stage2) angle2: %f\n", angle * 180 / M_PI);
	angle_to_points_2d(angle, sqrtf(	(delta_y * delta_y) + \
												((buffer->z - rotation_point->z) * (buffer->z - rotation_point->z)) ), \
												rotation_point->y, rotation_point->z, &(buffer->y), &(buffer->z));
	printf("(Stage2) buffer->x: %11.6f, buffer->y: %11.6f, buffer->z: %11.6f\n", buffer->x, buffer->y, buffer->z);
	
	return;
}
float DEBUG_points_to_angle_2d(float x1, float y1, float x2, float y2) {	
	x2 -= x1;
	y2 -= y1;
	printf("pta x2: %f, y2: %f\n", x2, y2);
	if (x2 == 0.0) {
		if (y2 < 0.0) {
			return M_PI;
		} else {
			return 0.0;
		}
	} else {
		if (y2 == 0.0) {
			if (x2 < 0.0) {
				return -M_PI_2;
			} else {
				return M_PI_2;
			}
		} else {
			if (y2 < 0.0) {
				if (x2 < 0.0) {
					return -M_PI_2 - atanf(y2 / x2);
				} else {
					return M_PI_2 - atanf(y2 / x2);
				}
			} else {
				return atanf(x2 / y2);
			}
		}
	}
	return 0.0;
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
	/*
	x2 -= x1;
	y2 -= y1;
	if (x2 == 0.0) {
		if (y2 < 0.0) {
			return M_PI;
		} else {
			return 0.0;
		}
	} else {
		if (y2 == 0.0) {
			if (x2 < 0.0) {
				return -M_PI_2;
			} else {
				return M_PI_2;
			}
		} else {
			if (y2 < 0.0) {
				if (x2 < 0.0) {
					return -M_PI_2 - atanf(y2 / x2);
				} else {
					return M_PI_2 - atanf(y2 / x2);
				}
			} else {
				return atanf(x2 / y2);
			}
		}
	}
	return 0.0;
	*/
}
void angle_to_points_2d(float angle, float magnitude, float x1, float y1, float* x2, float* y2) {
	*x2 = x1 + sinf(angle) * magnitude;
	*y2 = y1 + cosf(angle) * magnitude;
	return;
}
/*
unsigned int greater_than_point(struct matrix* po, struct matrix* p1, struct matrix* p2) {
	struct matrix tmp_vect1;
	struct matrix tmp_vect2;
	sub_m(po, p1, &tmp_vect1);
	sub_m(po, p2, &tmp_vect2);
	if (tmp_vect1.x > tmp_vect2.x && tmp_vect1.y < tmp_vect2.y && tmp_vect1.z < tmp_vect2.z) {
		return 1;
	}
	return 0;
}
unsigned int less_than_point(struct matrix* po, struct matrix* p1, struct matrix* p2) {
	struct matrix tmp_vect1;
	struct matrix tmp_vect2;
	sub_m(po, p1, &tmp_vect1);
	sub_m(po, p2, &tmp_vect2);
	if (tmp_vect1.x < tmp_vect2.x && tmp_vect1.y < tmp_vect2.y && tmp_vect1.z < tmp_vect2.z) {
		return 1;
	}
	return 0;
}
*/
void add_m(struct matrix* m1, struct matrix* m2, struct matrix* mr) {
	mr->x = m1->x + m2->x;
	mr->y = m1->y + m2->y;
	mr->z = m1->z + m2->z;
	return;
}
void sub_m(struct matrix* m1, struct matrix* m2, struct matrix* mr) {
	mr->x = m1->x - m2->x;
	mr->y = m1->y - m2->y;
	mr->z = m1->z - m2->z;
	return;
}
