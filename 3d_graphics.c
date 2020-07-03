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

unsigned int is_in_fov(struct camera* c, struct matrix* m, struct matrix* buffer);
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
	
	errno = 0;
	ptr = mmap(ptr, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
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
	
	uint32_t pt_colors[8];
	pt_colors[ 0] = 0x00FFFFFF;
	pt_colors[ 1] = 0x0000FFFF;
	pt_colors[ 2] = 0x00FF00FF;
	pt_colors[ 3] = 0x00FFFF00;
	pt_colors[ 4] = 0x000000FF;
	pt_colors[ 5] = 0x0000FF00;
	pt_colors[ 6] = 0x00FF0000;
	pt_colors[ 7] = 0x007F7F7F;
	
	cam.location.x =   0.0;
	cam.location.y =   0.0;
	cam.location.z = -10.0;
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
	
	struct matrix buffer;
	uint32_t color = 0;
	unsigned int i = 0;
	//unsigned int a = 0;
	unsigned int x = 0;
	unsigned int y = 0;
	while (i < 8) {
		//printf("\n------------------------------\n\n");
		//printf("ia: %d\n", i);
		if (is_in_fov(&cam, &(box.lines[i].p0), &buffer)) {
			/*
			color = 0x00FFFFFF;
			x = 0;
			while (x < 20) {
				y = 0;
				while (y < 20) {
					mcolor[(y * vinfo.xres) + x + a] = color;
					y++;
				}
				x++;
			}
			*/
			
			//color = 0x0000FF00;
			color = pt_colors[i];
			x = buffer.x / cam.h_fov * vinfo.xres;
			y = buffer.y / cam.v_fov * vinfo.yres;
			mcolor[y * vinfo.xres + x] = color;
			
			//printf("ia: Success\n", i);
		}// else {
			//printf("ia: Failed\n", i);
		//}
		//printf("\n");
		//a += 40;
		
		//printf("\n------------------------------\n\n");
		//printf("ib: %d\n", i);
		/*
		if (is_in_fov(&cam, &(box.lines[i].p1), &buffer)) {
			color = 0x00FFFFFF;
			x = 0;
			while (x < 20) {
				y = 0;
				while (y < 20) {
					mcolor[(y * vinfo.xres) + x + a] = color;
					y++;
				}
				x++;
			}
			
			color = 0x0000FF00;
			x = buffer.x * vinfo.xres / cam.h_fov;
			y = buffer.y * vinfo.yres / cam.v_fov;
			mcolor[y * vinfo.xres + x] = color;
			
			//printf("ib: Success\n", i);
		}// else {
			//printf("ib: Failed\n", i);
		//}
		*/
		//printf("\n");
		//a += 40;
		
		i++;
	}
	
	sleep(1);
	
	end:
	
	munmap(ptr, screensize);
	
	close(fd);
	
	return 0;
}

unsigned int is_in_fov(struct camera* c, struct matrix* m, struct matrix *buffer) {
	// Translate point(struct matrix* m) to the camera angles
	struct matrix buffer2;
	struct matrix rotation_delta;
	rotation_delta.z = 0.0;
	rotation_delta.y = points_to_angle_2d(c->location.x, c->location.z, c->looking_at.x, c->looking_at.z);
	rotation_delta.x = points_to_angle_2d(c->location.y, c->location.z, c->looking_at.y, c->looking_at.z);
	translate_rotation(m, &(c->location), &rotation_delta, &buffer2);
	
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
	h_xslope = tan((c->h_fov * M_PI) / 360.0);
	h_yslope = tan((c->v_fov * M_PI) / 360.0);
	l_xslope = -h_xslope;
	l_yslope = -h_yslope;
	h_xoffset = c->location.x - (h_xslope * c->location.z);
	h_yoffset = c->location.y - (h_yslope * c->location.z);
	l_xoffset = c->location.y - (l_yslope * c->location.z);
	l_yoffset = c->location.x - (l_xslope * c->location.z);
	
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
	buffer->x = points_to_angle_2d(c->location.x, c->location.z, buffer2.x, buffer2.z) * 180 / M_PI + (c->h_fov / 2);
	buffer->y = points_to_angle_2d(c->location.y, c->location.z, buffer2.y, buffer2.z) * 180 / M_PI + (c->v_fov / 2);
	buffer->z = 0.0; // This value is not used.
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
	
	// Translate Around Y-Axis
	angle = points_to_angle_2d(rotation_point->x, rotation_point->z, target_point->x, target_point->z);
	angle -= rotation_delta->y;
	angle_to_points_2d(angle, sqrtf(	(target_point->x - rotation_point->x) * (target_point->x - rotation_point->x) + \
												(target_point->z - rotation_point->z) * (target_point->z - rotation_point->z) ), \
												rotation_point->x, rotation_point->z, &(buffer->x), &(buffer->z));
	
	// Translate Around X-Axis
	angle = points_to_angle_2d(rotation_point->y, rotation_point->z, target_point->y, target_point->z);
	angle -= rotation_delta->x;
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
