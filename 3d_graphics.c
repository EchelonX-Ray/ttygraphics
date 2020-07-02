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
#include <stdint.h>
#include <math.h>

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
	struct matrix location;
	struct matrix looking_at;
	float h_fov;
	float v_fov;
};
struct slope {
	unsigned int type;
	float slope;
	float offset;
};

unsigned int is_in_fov(struct camera* c, struct matrix* m);
void translate_rotation(struct matrix* point, struct matrix* rotation_point, struct matrix* rotation_delta, struct matrix* buffer);
float points_to_angle_2d(float x1, float y1, float x2, float y2);
void angle_to_points_2d(float angle, float magnitude, float x1, float y1, float* x2, float* y2);
//unsigned int greater_than_point(struct matrix* po, struct matrix* p1, struct matrix* p2);
//unsigned int less_than_point(struct matrix* po, struct matrix* p1, struct matrix* p2);
void add_m(struct matrix* m1, struct matrix* m2, struct matrix* mr);
void sub_m(struct matrix* m1, struct matrix* m2, struct matrix* mr);
//void mul_m(struct matrix* m1, struct matrix* m2, struct matrix* mr);
//void div_m(struct matrix* m1, struct matrix* m2, struct matrix* mr);

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
	sigemptyset(&(new_action.sa_mask));
	new_action.sa_flags = SA_NOCLDSTOP;
	new_action.sa_restorer = 0;
	struct sigaction old_action;
	
	if (sigaction(SIGCHLD, &new_action, &old_action) != 0) {
		printf("SIGCHLD Action Flag Change Failed!\n");
		return 1;
	}
	
	signed int fd;
	unsigned long request;
	signed int ret_val = 0;
	
	fd = 1;
	//fd = open("/dev/tty6", O_RDWR);
	if (fd < 0) {
		printf("Invaild fd.  Possible open() failure!  Stage-1\n");
		return 4;
	}
	
	//ret_val = ioctl(fd, KDSETMODE, 1);
	if (ret_val != 0) {
		printf("Failure Setting TTY Mode: To Graphical!\n");
		printf("errno: %s\n", strerror(errno));
		return 3;
	}
	//close(fd);
	
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
		//ret_val = ioctl(fd, KDSETMODE, 0);
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
	
	if (vinfo.bits_per_pixel != 32) {
		printf("Wrong bits per pixel!\n");
		return 6;
	}
	
	uint32_t* mcolor;
	void* ptr = 0;
	unsigned int screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
	
	ptr = mmap(ptr, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (ptr == MAP_FAILED || ptr == 0) {
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
	box.lines[11].p1.x = -50.0;
	box.lines[11].p1.y = -1.0;
	box.lines[11].p1.z = +1.0;
	
	cam.location.x =   0.0;
	cam.location.y =   0.0;
	cam.location.z = -10.0;
	cam.looking_at.x =   0.0;
	cam.looking_at.y =   0.0;
	cam.looking_at.z =   0.0;
	
	/*
	struct matrix cam_vect_min;
	struct matrix cam_vect_max;
	
	cam_vect_min.z = 1.0;
	*/
	
	float max_fov_degrees = 90.0;
	unsigned int width = vinfo.xres;
	unsigned int height = vinfo.yres;
	
	if (width > height) {
		cam.h_fov = max_fov_degrees;
		cam.v_fov = (max_fov_degrees * height) / width;
	} else {
		cam.v_fov = max_fov_degrees;
		cam.h_fov = (max_fov_degrees * width) / height;
	}
	
	// Start: Debuging code
	/*
	printf("HFOV: %f, VFOV: %f\n\n", cam.h_fov, cam.v_fov);
	
	cam.looking_at.x =   0.0;
	cam.looking_at.y =   0.0;
	cam.looking_at.z =  -2.0;
	is_in_fov(&cam, &box.lines[ 0].p0);
	
	cam.looking_at.x =   0.0;
	cam.looking_at.y =  -0.1;
	cam.looking_at.z =  -2.0;
	is_in_fov(&cam, &box.lines[ 0].p0);
	
	cam.looking_at.x =  -0.1;
	cam.looking_at.y =   0.0;
	cam.looking_at.z =  -0.0;
	is_in_fov(&cam, &box.lines[ 0].p0);
	
	cam.looking_at.x =   0.0;
	cam.looking_at.y =   1.0;
	cam.looking_at.z =  -1.0;
	is_in_fov(&cam, &box.lines[ 0].p0);
	
	cam.looking_at.x =   1.0;
	cam.looking_at.y =   1.0;
	cam.looking_at.z =  -2.0;
	is_in_fov(&cam, &box.lines[ 0].p0);
	goto end;
	*/
	// End: Debuging code
	
	/*
	cam_vect_max.z = cam_vect_min.z;
	
	unsigned int width = vinfo.xres;
	unsigned int height = vinfo.yres;
	if (height < width) {
		cam_vect_min.x = -tanf(max_fov_degrees / 2.0) * cam_vect_min.z;
		cam_vect_min.y = +tanf((max_fov_degrees * (height / width)) / 2.0) * cam_vect_min.z;
		cam_vect_max.x = +tanf(max_fov_degrees / 2.0) * cam_vect_max.z;
		cam_vect_max.y = -tanf((max_fov_degrees * (height / width)) / 2.0) * cam_vect_max.z;
	} else {
		cam_vect_min.y = +tanf(max_fov_degrees / 2.0) * cam_vect_min.z;
		cam_vect_min.x = -tanf((max_fov_degrees * (width / height)) / 2.0) * cam_vect_min.z;
		cam_vect_max.y = -tanf(max_fov_degrees / 2.0) * cam_vect_max.z;
		cam_vect_max.x = +tanf((max_fov_degrees * (width / height)) / 2.0) * cam_vect_max.z;
	}
	*/
	
	uint32_t color = 0x00FFFFFF;
	unsigned int i = 0;
	unsigned int a = 0;
	unsigned int x = 0;
	unsigned int y = 0;
	while (i < 12) {
		printf("\n------------------------------\n\n");
		printf("ia: %d\n\n", i);
		if (is_in_fov(&cam, &(box.lines[i].p0))) {
			x = 0;
			while (x < 20) {
				y = 0;
				while (y < 20) {
					mcolor[(y * vinfo.xres) + x + a] = color;
					y++;
				}
				x++;
			}
			printf("ia: Success\n", i);
		} else {
			printf("ia: Failed\n", i);
		}
		a += 40;
		
		printf("\n------------------------------\n\n");
		printf("ib: %d\n\n", i);
		if (is_in_fov(&cam, &(box.lines[i].p1))) {
			x = 0;
			while (x < 20) {
				y = 0;
				while (y < 20) {
					mcolor[(y * vinfo.xres) + x + a] = color;
					y++;
				}
				x++;
			}
			printf("ib: Success\n", i);
		} else {
			printf("ib: Failed\n", i);
		}
		a += 40;
		
		i++;
	}
	
	sleep(1);
	
	end:
	
	munmap(ptr, screensize);
	
	close(fd);
	
	return 0;
}

unsigned int is_in_fov(struct camera* c, struct matrix* m) {
	//tanf(c.h_fov / 2.0) * cam_vect_min.z;
	
	struct matrix low_angle;
	struct matrix high_angle;
	
	// TODO: calculate low_angle and high_angle
	float x_angle = 0.0;
	float y_angle = 0.0;
	//float z_angle = 0.0;
	//c->h_fov;
	//c->v_fov;
	float delta_x;
	float delta_y;
	float delta_z;
	//slope_x = c->location.x c->looking_at.x c->location.y c->looking_at.y
	delta_x = c->looking_at.x - c->location.x;
	delta_y = c->looking_at.y - c->location.y;
	delta_z = c->looking_at.z - c->location.z;
	if (delta_y == 0) {
		x_angle = 0.0;
	} else {
		x_angle = atanf(delta_y / sqrtf((delta_x * delta_x) + (delta_z * delta_z)));
	}
	if (delta_x == 0) {
		y_angle = 0.0;
	} else {
		y_angle = atanf(delta_x / delta_z);
	}
	if (delta_z < 0) {
		//x_angle = -x_angle;
		y_angle += M_PI;
		if (y_angle > 180.0) {
			y_angle -= 360.0;
		}
	}
	
	printf("x_angle: %f\n", x_angle * 180.0 / M_PI);
	printf("y_angle: %f\n", y_angle * 180.0 / M_PI);
	printf("\n");
	
	/*
	float l_xangle;
	float h_xangle;
	float l_yangle;
	float h_yangle;
	l_yangle = 0.0;
	h_yangle = 0.0;
	l_xangle = x_angle - ((c->v_fov * M_PI) / 360.0);
	h_xangle = x_angle + ((c->v_fov * M_PI) / 360.0);
	if (l_xangle > M_PI_2) {
		l_xangle = M_PI - l_xangle;
		l_yangle += M_PI;
	} else if (l_xangle < -90.0) {
		l_xangle = M_PI + l_xangle;
		l_yangle += M_PI;
	}
	if (h_xangle > M_PI_2) {
		h_xangle = M_PI - h_xangle;
		h_yangle += M_PI;
	} else if (h_xangle < -90.0) {
		h_xangle = M_PI + h_xangle;
		h_yangle += M_PI;
	}
	l_yangle += y_angle - ((c->h_fov * M_PI) / 360.0);
	h_yangle += y_angle + ((c->h_fov * M_PI) / 360.0);
	low_angle.z = c->location.z + cosf(l_yangle);
	low_angle.x = c->location.x + sinf(l_yangle);
	low_angle.y = c->location.y + sinf(l_xangle);
	high_angle.z = c->location.z + cosf(h_yangle);
	high_angle.x = c->location.x + sinf(h_yangle);
	high_angle.y = c->location.y + sinf(h_xangle);
	
	printf("l_xangle = %f, l_yangle = %f\n", l_xangle * 180 / M_PI, l_yangle * 180 / M_PI);
	printf("h_xangle = %f, h_yangle = %f\n", h_xangle * 180 / M_PI, h_yangle * 180 / M_PI);
	printf("\n");
	printf("low_angle: x = %f, y = %f, z = %f\n", low_angle.x, low_angle.y, low_angle.z);
	printf("high_angle: x = %f, y = %f, z = %f\n", high_angle.x, high_angle.y, high_angle.z);
	printf("\n");
	*/
	
	
	struct matrix buffer;
	struct matrix rotation_delta;
	rotation_delta.x = x_angle;
	rotation_delta.y = y_angle;
	rotation_delta.z = 0.0;
	translate_rotation(m, &(c->location), &rotation_delta, &buffer);
	
	float h_xslope;
	float h_yslope;
	float l_xslope;
	float l_yslope;
	float h_xoffset;
	float h_yoffset;
	float l_xoffset;
	float l_yoffset;
	h_xslope = tan((c->h_fov * M_PI) / 360.0);
	h_yslope = tan((c->v_fov * M_PI) / 360.0);
	l_xslope = -h_xslope;
	l_yslope = -h_yslope;
	h_xoffset = c->location.x - (h_xslope * c->location.z);
	h_yoffset = c->location.y - (h_yslope * c->location.z);
	l_xoffset = c->location.y - (l_yslope * c->location.z);
	l_yoffset = c->location.x - (l_xslope * c->location.z);
	
	printf("m->x: %f, m->y: %f, m->z: %f\n", m->x, m->y, m->z);
	printf("b->x: %f, b->y: %f, b->z: %f\n", buffer.x, buffer.y, buffer.z);
	
	if (buffer.x > (h_xslope * buffer.z) + h_xoffset) {
		return 0;
	}
	if (buffer.x < (l_xslope * buffer.z) + l_xoffset) {
		return 0;
	}
	if (buffer.y > (h_yslope * buffer.z) + h_yoffset) {
		return 0;
	}
	if (buffer.y < (l_yslope * buffer.z) + l_yoffset) {
		return 0;
	}
	
	/*
	// Calculate x-y y-z x-z for low_angle
	//struct slope la_x_y;
	struct slope la_y_z;
	struct slope la_x_z;
	// Calc x-y and x-z
	if (c->location.x == low_angle.x) {
		//la_x_y.type = 1;
		la_x_z.type = 1;
		//la_x_y.offset = c->location.x;
		la_x_z.offset = c->location.x;
	} else {
		//la_x_y.type = 0;
		la_x_z.type = 0;
		//la_x_y.slope = (c->location.y - low_angle.y) / (c->location.x - low_angle.x);
		la_x_z.slope = (c->location.z - low_angle.z) / (c->location.x - low_angle.x);
		//la_x_y.offset = c->location.y - (la_x_y.slope * c->location.x);
		la_x_z.offset = c->location.z - (la_x_z.slope * c->location.x);
	}
	// Calc y-z
	if (c->location.y == low_angle.y) {
		la_y_z.type = 1;
		la_y_z.offset = c->location.y;
	} else {
		la_y_z.type = 0;
		la_y_z.slope = (c->location.z - low_angle.z) / (c->location.y - low_angle.y);
		la_y_z.offset = c->location.z - (la_y_z.slope * c->location.y);
	}
	
	// Calculate x-y y-z x-z for high_angle
	//struct slope ha_x_y;
	struct slope ha_y_z;
	struct slope ha_x_z;
	// Calc x-y and x-z
	if (c->location.x == high_angle.x) {
		//ha_x_y.type = 1;
		ha_x_z.type = 1;
		//ha_x_y.offset = c->location.x;
		ha_x_z.offset = c->location.x;
	} else {
		//ha_x_y.type = 0;
		ha_x_z.type = 0;
		//ha_x_y.slope = (c->location.y - high_angle.y) / (c->location.x - high_angle.x);
		ha_x_z.slope = (c->location.z - high_angle.z) / (c->location.x - high_angle.x);
		//ha_x_y.offset = c->location.y - (ha_x_y.slope * c->location.x);
		ha_x_z.offset = c->location.z - (ha_x_z.slope * c->location.x);
	}
	// Calc y-z
	if (c->location.y == high_angle.y) {
		ha_y_z.type = 1;
		ha_y_z.offset = c->location.y;
	} else {
		ha_y_z.type = 0;
		ha_y_z.slope = (c->location.z - high_angle.z) / (c->location.y - high_angle.y);
		ha_y_z.offset = c->location.z - (ha_y_z.slope * c->location.y);
	}
	
	if (ha_y_z) {
	}
	
	// Determine whether the calculation should be less-than or greater-than 
	// per each slope based on the point the camera is pointing at.
	// Do the low_angle
	if (la_x_y.type == 0) {
		if (c->looking_at.y < (la_x_y.slope * c->looking_at.x) + la_x_y.offset) {
			if (m->y > (la_x_y.slope * m->x) + la_x_y.offset) {
				printf("TraceA\n");
				return 0;
			}
		} else {
			if (m->y < (la_x_y.slope * m->x) + la_x_y.offset) {
				printf("[DEBUG] m->y: %f\n", m->y);
				printf("[DEBUG] m->x: %f\n", m->x);
				printf("[DEBUG] m->z: %f\n", m->z);
				printf("[DEBUG] la_x_y.slope: %f\n", la_x_y.slope);
				printf("[DEBUG] la_x_y.offset: %f\n", la_x_y.offset);
				printf("TraceB\n");
				return 0;
			}
		}
	} else {
		if (c->looking_at.x < la_x_y.offset) {
			if (m->x > la_x_y.offset) {
				printf("TraceC\n");
				return 0;
			}
		} else if (c->looking_at.x > la_x_y.offset) {
			if (m->x < la_x_y.offset) {
				printf("TraceD\n");
				return 0;
			}
		}
	}
	if (la_y_z.type == 0) {
		if (c->looking_at.z < (la_y_z.slope * c->looking_at.y) + la_y_z.offset) {
			if (m->z > (la_y_z.slope * m->y) + la_y_z.offset) {
				printf("TraceE\n");
				return 0;
			}
		} else if (c->looking_at.z > (la_y_z.slope * c->looking_at.y) + la_y_z.offset) {
			if (m->z < (la_y_z.slope * m->y) + la_y_z.offset) {
				printf("TraceF\n");
				return 0;
			}
		}
	} else {
		if (c->looking_at.y < la_y_z.offset) {
			if (m->y > la_y_z.offset) {
				printf("TraceG\n");
				return 0;
			}
		} else if (c->looking_at.y > la_y_z.offset) {
			if (m->y < la_y_z.offset) {
				printf("TraceH\n");
				return 0;
			}
		}
	}
	if (la_x_z.type == 0) {
		if (c->looking_at.z < (la_x_z.slope * c->looking_at.x) + la_x_z.offset) {
			if (m->z > (la_x_z.slope * m->x) + la_x_z.offset) {
				printf("TraceI\n");
				return 0;
			}
		} else if (c->looking_at.z > (la_x_z.slope * c->looking_at.x) + la_x_z.offset) {
			if (m->z < (la_x_z.slope * m->x) + la_x_z.offset) {
				printf("TraceJ\n");
				return 0;
			}
		}
	} else {
		if (c->looking_at.x < la_x_z.offset) {
			if (m->x > la_x_z.offset) {
				printf("TraceK\n");
				return 0;
			}
		} else if (c->looking_at.x > la_x_z.offset) {
			if (m->x < la_x_z.offset) {
				printf("TraceL\n");
				return 0;
			}
		}
	}
	// Do the high_angle
	if (ha_x_y.type == 0) {
		if (c->looking_at.y < (ha_x_y.slope * c->looking_at.x) + ha_x_y.offset) {
			if (m->y > (ha_x_y.slope * m->x) + ha_x_y.offset) {
				printf("TraceM\n");
				return 0;
			}
		} else if (c->looking_at.y > (ha_x_y.slope * c->looking_at.x) + ha_x_y.offset) {
			if (m->y < (ha_x_y.slope * m->x) + ha_x_y.offset) {
				printf("TraceN\n");
				return 0;
			}
		}
	} else {
		if (c->looking_at.x < ha_x_y.offset) {
			if (m->x > ha_x_y.offset) {
				printf("TraceO\n");
				return 0;
			}
		} else if ((c->looking_at.x > ha_x_y.offset)) {
			if (m->x < ha_x_y.offset) {
				printf("TraceP\n");
				return 0;
			}
		}
	}
	if (ha_y_z.type == 0) {
		if (c->looking_at.z < (ha_y_z.slope * c->looking_at.y) + ha_y_z.offset) {
			if (m->z > (ha_y_z.slope * m->y) + ha_y_z.offset) {
				printf("TraceQ\n");
				return 0;
			}
		} else if (c->looking_at.z > (ha_y_z.slope * c->looking_at.y) + ha_y_z.offset) {
			if (m->z < (ha_y_z.slope * m->y) + ha_y_z.offset) {
				printf("TraceR\n");
				return 0;
			}
		}
	} else {
		if (c->looking_at.y < ha_y_z.offset) {
			if (m->y > ha_y_z.offset) {
				printf("TraceS\n");
				return 0;
			}
		} else if (c->looking_at.y > ha_y_z.offset) {
			if (m->y < ha_y_z.offset) {
				printf("TraceT\n");
				return 0;
			}
		}
	}
	if (ha_x_z.type == 0) {
		if (c->looking_at.z < (ha_x_z.slope * c->looking_at.x) + ha_x_z.offset) {
			if (m->z > (ha_x_z.slope * m->x) + ha_x_z.offset) {
				printf("TraceU\n");
				return 0;
			}
		} else if (c->looking_at.z > (ha_x_z.slope * c->looking_at.x) + ha_x_z.offset) {
			if (m->z < (ha_x_z.slope * m->x) + ha_x_z.offset) {
				printf("TraceV\n");
				return 0;
			}
		}
	} else {
		if (c->looking_at.x < ha_x_z.offset) {
			if (m->x > ha_x_z.offset) {
				printf("TraceW\n");
				return 0;
			}
		} else if (c->looking_at.x > ha_x_z.offset) {
			if (m->x < ha_x_z.offset) {
				printf("TraceX\n");
				return 0;
			}
		}
	}
	*/
	
	return 1;
}
void translate_rotation(struct matrix* target_point, struct matrix* rotation_point, struct matrix* rotation_delta, struct matrix* buffer) {
	float delta_x = target_point->x - rotation_point->x;
	float delta_y = target_point->y - rotation_point->y;
	float delta_z = target_point->z - rotation_point->z; //TODO
	float tmp_var;
	float angle;
	buffer->x = 0.0;
	buffer->y = 0.0;
	buffer->z = 0.0;
	
	printf("target_point-> x:%f, y:%f, z:%f\n", target_point->x, target_point->y, target_point->z);
	printf("rotation_point-> x:%f, y:%f, z:%f\n", rotation_point->x, rotation_point->y, rotation_point->z);
	printf("rotation_delta-> x:%f, y:%f, z:%f\n", rotation_delta->x * 180 / M_PI, rotation_delta->y * 180 / M_PI, rotation_delta->z * 180 / M_PI);
	printf("delta_x: %f, delta_y: %f, delta_z: %f\n\n", delta_x, delta_y, delta_z);
	
	// Translate Around Y-Axis
	angle = points_to_angle_2d(rotation_point->x, rotation_point->z, target_point->x, target_point->z);
	printf("com angle y: %f\n", angle * 180.0 / M_PI);
	angle -= rotation_delta->y;
	printf("sum angle y: %f\n", angle * 180.0 / M_PI);
	angle_to_points_2d(angle, sqrtf(	(target_point->x - rotation_point->x) * (target_point->x - rotation_point->x) + \
												(target_point->z - rotation_point->z) * (target_point->z - rotation_point->z) ), \
												rotation_point->x, rotation_point->z, &(buffer->x), &(buffer->z));
	printf("\nbuffer->x: %f, buffer->y: %f, buffer->z: %f\n\n", buffer->x, buffer->y, buffer->z);
	
	// Translate Around X-Axis
	angle = points_to_angle_2d(rotation_point->y, rotation_point->z, target_point->y, target_point->z);
	printf("com angle x: %f\n", angle * 180.0 / M_PI);
	angle -= rotation_delta->x;
	printf("sum angle x: %f\n", angle * 180.0 / M_PI);
	angle_to_points_2d(angle, sqrtf(	(target_point->y - rotation_point->y) * (target_point->y - rotation_point->y) + \
												(buffer->z - rotation_point->z) * (buffer->z - rotation_point->z) ), \
												rotation_point->y, rotation_point->z, &(buffer->y), &(buffer->z));
	
	return;
}
float points_to_angle_2d(float x1, float y1, float x2, float y2) {
	x2 -= x1;
	y2 -= y1;
	if (x2 == 0) {
		x1 = atanf(0);
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
