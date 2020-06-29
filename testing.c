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
	box.lines[11].p1.x = -1.0;
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
		if (is_in_fov(&cam, &box.lines[i].p0)) {
			x = 0;
			while (x < 20) {
				y = 0;
				while (y < 20) {
					mcolor[(y * vinfo.xres) + x + a] = color;
					y++;
				}
				x++;
			}
		}
		a += 40;
		
		if (is_in_fov(&cam, &box.lines[i].p1)) {
			x = 0;
			while (x < 20) {
				y = 0;
				while (y < 20) {
					mcolor[(y * vinfo.xres) + x + a] = color;
					y++;
				}
				x++;
			}
		}
		a += 40;
		
		i++;
	}
	
	sleep(3);
	
	munmap(ptr, screensize);
	
	close(fd);
	
	return 0;
}

unsigned int is_in_fov(struct camera* c, struct matrix* m) {
	//tanf(c.h_fov / 2.0) * cam_vect_min.z;
	
	struct matrix low_angle;
	struct matrix high_angle;
	
	// TODO calculate low_angle and high_angle
	
	// Calculate x-y y-z x-z for low_angle
	struct slope la_x_y;
	struct slope la_y_z;
	struct slope la_x_z;
	
	if (c->location.x == low_angle.x) {
		// TODO: Represent x = c.location.x
		la_x_y.type = 1;
		la_x_z.type = 1;
		la_x_y.offset = c->location.x;
		la_x_z.offset = c->location.z;
	} else {
		la_x_y.type = 0;
		la_x_z.type = 0;
		
		la_x_y.slope = (c->location.y - low_angle.y) / (c->location.x - low_angle.x);
		la_x_z.slope = (c->location.z - low_angle.z) / (c->location.x - low_angle.x);
		
		la_x_y.offset = c->location.y - (la_x_y.slope * c->location.x);
		la_x_z.offset = c->location.z - (la_x_z.slope * c->location.x);
	}
	
	// Calculate x-y y-z x-z for high_angle
	
	return 0;
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
