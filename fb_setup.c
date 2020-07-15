#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <linux/fb.h>
//#include <drm/drm.h>
//#include <drm/drm_mode.h>
#include <libdrm/drm.h>
#include <libdrm/drm_mode.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>

struct con_fb {
	void *fb_ffb;
	void *fb_bfb;
	unsigned int line_length;
	unsigned int width;
	unsigned int height;
	unsigned int size;
	unsigned int a_length;
	unsigned int type;
	uint64_t crtc_id;
	uint32_t ffb_id;
	uint32_t bfb_id;
	uint32_t ffb_db;
	uint32_t bfb_db;
};

void clean_up(signed int fd, struct con_fb* fbs) {
	unsigned int ret_val = 0;
	unsigned int errno_bak = 0;
	unsigned int i;
	
	if (fbs != 0) {
		for(i = 0; i < fbs[0].a_length; i++) {
			if (fbs[i].type == 2) {
				if (fbs[i].fb_ffb > fbs[i].fb_bfb) {
					fbs[i].fb_ffb = fbs[i].fb_bfb;
				}
				ret_val = munmap(fbs[i].fb_ffb, fbs[i].size);
				if (ret_val == -1) {
					errno_bak = errno;
					fprintf(stderr, "[%d] Type2 - munmap(fbs[i].fb_ffb, fbs[i].size); fail (%d)\n", i, errno_bak);
					fprintf(stderr, "errno: %s\n", strerror(errno_bak));
				}
				continue;
			}
			
			if (fbs[i].fb_ffb == fbs[i].fb_bfb) {
				ret_val = munmap(fbs[i].fb_ffb, fbs[i].size);
				if (ret_val == -1) {
					errno_bak = errno;
					fprintf(stderr, "[%d] NO DB - munmap(fbs[i].fb_ffb, fbs[i].size); fail (%d)\n", i, errno_bak);
					fprintf(stderr, "errno: %s\n", strerror(errno_bak));
				}
			} else {
				ret_val = munmap(fbs[i].fb_ffb, fbs[i].size);
				if (ret_val == -1) {
					errno_bak = errno;
					fprintf(stderr, "[%d] munmap(fbs[i].fb_ffb, fbs[i].size); fail (%d)\n", i, errno_bak);
					fprintf(stderr, "errno: %s\n", strerror(errno_bak));
				}
				ret_val = munmap(fbs[i].fb_bfb, fbs[i].size);
				if (ret_val == -1) {
					errno_bak = errno;
					fprintf(stderr, "[%d] munmap(fbs[i].fb_bfb, fbs[i].size); fail (%d)\n", i, errno_bak);
					fprintf(stderr, "errno: %s\n", strerror(errno_bak));
				}
			}
			
			struct drm_mode_destroy_dumb ddb;
			if (fbs[i].ffb_db == fbs[i].bfb_db) {
				ddb.handle = fbs[i].ffb_db;
				ret_val = ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &ddb);
				if (ret_val == -1) {
					errno_bak = errno;
					fprintf(stderr, "[%d] NO DB - Failure 1: ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &ddb); (%d)\n", i, errno_bak);
					fprintf(stderr, "errno: %s\n", strerror(errno_bak));
				}
			} else {
				ddb.handle = fbs[i].ffb_db;
				ret_val = ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &ddb);
				if (ret_val == -1) {
					errno_bak = errno;
					fprintf(stderr, "[%d] Failure 1: ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &ddb); (%d)\n", i, errno_bak);
					fprintf(stderr, "errno: %s\n", strerror(errno_bak));
				}
				ddb.handle = fbs[i].bfb_db;
				ret_val = ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &ddb);
				if (ret_val == -1) {
					errno_bak = errno;
					fprintf(stderr, "[%d] Failure 2: ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &ddb); (%d)\n", i, errno_bak);
					fprintf(stderr, "errno: %s\n", strerror(errno_bak));
				}
			}
		}
		free(fbs);
	}
	return;
}

void _clean_up(	signed int fd, 
						struct con_fb* fbs, 
						struct drm_mode_get_connector* conn, 
						struct drm_mode_card_res* res, 
						unsigned int loop_max, 
						unsigned int destroy_dumb_buffers, 
						unsigned int munmap_fbs, 
						unsigned int drop_master	) {
	
	unsigned int ret_val = 0;
	unsigned int errno_bak = 0;
	
	unsigned int i = 0;
	while (i < loop_max) {
		if (munmap_fbs == 1) {
			if (fbs[i].fb_bfb == fbs[i].fb_ffb) {
				if (fbs[i].fb_bfb != 0 && fbs[i].fb_bfb != (void*)-1) {
					ret_val = munmap(fbs[i].fb_bfb, fbs[i].size);
					if (ret_val == -1) {
						errno_bak = errno;
						fprintf(stderr, "[%d] NO DB - munmap(fbs[i].fb_bfb, fbs[i].size); fail (%d)\n", i, errno_bak);
						fprintf(stderr, "errno: %s\n", strerror(errno_bak));
					}
				}
			} else {
				if (fbs[i].fb_bfb != 0 && fbs[i].fb_bfb != (void*)-1) {
					ret_val = munmap(fbs[i].fb_bfb, fbs[i].size);
					if (ret_val == -1) {
						errno_bak = errno;
						fprintf(stderr, "[%d] munmap(fbs[i].fb_bfb, fbs[i].size); fail (%d)\n", i, errno_bak);
						fprintf(stderr, "errno: %s\n", strerror(errno_bak));
					}
				}
				if (fbs[i].fb_ffb != 0 && fbs[i].fb_ffb != (void*)-1) {
					ret_val = munmap(fbs[i].fb_ffb, fbs[i].size);
					if (ret_val == -1) {
						errno_bak = errno;
						fprintf(stderr, "[%d] munmap(fbs[i].fb_ffb, fbs[i].size); fail (%d)\n", i, errno_bak);
						fprintf(stderr, "errno: %s\n", strerror(errno_bak));
					}
				}
			}
		}
		if (destroy_dumb_buffers == 1) {
			struct drm_mode_destroy_dumb ddb;
			if (fbs[i].ffb_db == fbs[i].bfb_db) {
				if (fbs[i].ffb_db != 0) {
					ddb.handle = fbs[i].ffb_db;
					ret_val = ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &ddb);
					if (ret_val == -1) {
						errno_bak = errno;
						fprintf(stderr, "[%d] NO DB - Failure 1: ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &ddb); (%d)\n", i, errno_bak);
						fprintf(stderr, "errno: %s\n", strerror(errno_bak));
					}
				}
			} else {
				if (fbs[i].ffb_db != 0) {
					ddb.handle = fbs[i].ffb_db;
					ret_val = ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &ddb);
					if (ret_val == -1) {
						errno_bak = errno;
						fprintf(stderr, "[%d] Failure 1: ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &ddb); (%d)\n", i, errno_bak);
						fprintf(stderr, "errno: %s\n", strerror(errno_bak));
					}
				}
				if (fbs[i].bfb_db != 0) {
					ddb.handle = fbs[i].bfb_db;
					ret_val = ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &ddb);
					if (ret_val == -1) {
						errno_bak = errno;
						fprintf(stderr, "[%d] Failure 2: ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &ddb); (%d)\n", i, errno_bak);
						fprintf(stderr, "errno: %s\n", strerror(errno_bak));
					}
				}
			}
		}
		if (conn[i].encoders_ptr) {
			free((void*)conn[i].encoders_ptr);
		}
		if (conn[i].modes_ptr) {
			free((void*)conn[i].modes_ptr);
		}
		if (conn[i].props_ptr) {
			free((void*)conn[i].props_ptr);
		}
		if (conn[i].prop_values_ptr) {
			free((void*)conn[i].prop_values_ptr);
		}
		i++;
	}
	
	if (res->fb_id_ptr) {
		free((void*)res->fb_id_ptr);
	}
	if (res->crtc_id_ptr) {
		free((void*)res->crtc_id_ptr);
	}
	if (res->connector_id_ptr) {
		free((void*)res->connector_id_ptr);
	}
	if (res->encoder_id_ptr) {
		free((void*)res->encoder_id_ptr);
	}
	if (conn) {
		free(conn);
	}
	if (fbs) {
		free(fbs);
	}
	
	if (drop_master == 1) {
		ret_val = ioctl(fd, DRM_IOCTL_DROP_MASTER, 0);
		if (ret_val == -1) {
			errno_bak = errno;
			fprintf(stderr, "Failure: ioctl(fd, DRM_IOCTL_DROP_MASTER, 0); (%d)\n", errno_bak);
			fprintf(stderr, "errno: %s\n", strerror(errno_bak));
		}
	}
	
	return;
}

void swap_buffers(signed int fd, struct con_fb* fbs, unsigned int i) {
	if (fbs == 0) {
		return;
	}
	if (fbs[i].fb_ffb == fbs[i].fb_bfb) {
		return;
	}
	
	unsigned int ret_val;
	unsigned int errno_bak;
	if (fbs[i].type == 1) {
		struct drm_mode_crtc_page_flip page_flip;
		memset(&page_flip, 0, sizeof(struct drm_mode_crtc_page_flip));
		
		page_flip.crtc_id = fbs[i].crtc_id;
		page_flip.fb_id = fbs[i].bfb_id;
		
		ret_val = ioctl(fd, DRM_IOCTL_MODE_PAGE_FLIP, &page_flip);
		if (ret_val == -1) {
			errno_bak = errno;
			fprintf(stderr, "Failure: ioctl(fd, DRM_IOCTL_MODE_PAGE_FLIP, &page_flip);\n");
			fprintf(stderr, "errno: %s\n", strerror(errno_bak));
		} else {
			register void *tmp_ptr;
			tmp_ptr = fbs[i].fb_ffb;
			fbs[i].fb_ffb = fbs[i].fb_bfb;
			fbs[i].fb_bfb = tmp_ptr;
			register uint64_t tmp_uint32_t;
			tmp_uint32_t = fbs[i].ffb_id;
			fbs[i].ffb_id = fbs[i].bfb_id;
			fbs[i].bfb_id = tmp_uint32_t;
			tmp_uint32_t = fbs[i].ffb_db;
			fbs[i].ffb_db = fbs[i].bfb_db;
			fbs[i].bfb_db = tmp_uint32_t;
		}
	} else if (fbs[i].type == 2) {
		struct fb_var_screeninfo vinfo;
		memset(&vinfo, 0, sizeof(struct fb_var_screeninfo));
		vinfo.xoffset = 0;
		if (fbs[i].ffb_db == 0) {
			vinfo.yoffset = fbs[i].bfb_db;
		} else {
			vinfo.yoffset = 0;
		}
		ret_val = ioctl(fd, FBIOPAN_DISPLAY, &vinfo);
		if (ret_val == -1) {
			errno_bak = errno;
			fprintf(stderr, "Error: Display Pan Failed.\n");
			fprintf(stderr, "errno: %s\n", strerror(errno_bak));
		} else {
			register void *tmp_ptr;
			tmp_ptr = fbs[i].fb_ffb;
			fbs[i].fb_ffb = fbs[i].fb_bfb;
			fbs[i].fb_bfb = tmp_ptr;
			if (fbs[i].ffb_db == 0) {
				fbs[i].ffb_db = fbs[i].bfb_db;
			} else {
				fbs[i].ffb_db = 0;
			}
		}
	}
	
	return;
}

void vsync_wait(signed int fd, struct con_fb* fbs, unsigned int force) {
	if (fbs == 0) {
		return;
	}
	unsigned int ret_val;
	unsigned int errno_bak;
	if (fbs[0].type == 1) {
		struct drm_wait_vblank_request vblank;
		memset(&vblank, 0, sizeof(struct drm_wait_vblank_request));
		
		vblank.type = _DRM_VBLANK_RELATIVE;
		vblank.sequence = 1;
		
		ret_val = ioctl(fd, DRM_IOCTL_WAIT_VBLANK, &vblank);
		if (ret_val == -1) {
			errno_bak = errno;
			fprintf(stderr, "Failure: ioctl(fd, DRM_IOCTL_MODE_PAGE_FLIP, &page_flip);\n");
			fprintf(stderr, "errno: %s\n", strerror(errno_bak));
		}
	} else if (fbs[0].type == 2 && force == 1) {
		ret_val = ioctl(fd, FBIO_WAITFORVSYNC, 0);
		if (ret_val == -1) {
			errno_bak = errno;
			fprintf(stderr, "Error: WAITFORVSYNC Failed.\n");
			fprintf(stderr, "errno: %s\n", strerror(errno_bak));
		}
	}
	return;
}

struct con_fb* try_drm_fb(signed int fd, unsigned int setup_doublebuffering) {
	unsigned int ret_val = 0;
	unsigned int errno_bak = 0;
	
	ret_val = ioctl(fd, DRM_IOCTL_SET_MASTER, 0);
	if (ret_val == -1) {
		errno_bak = errno;
		fprintf(stderr, "Failure: ioctl(fd, DRM_IOCTL_SET_MASTER, 0); (%d)\n", errno_bak);
		fprintf(stderr, "errno: %s\n", strerror(errno_bak));
	}
	
	struct drm_mode_card_res res;
	struct drm_mode_get_connector* conn;
	struct con_fb* fbs;
	
	memset(&res, 0, sizeof(struct drm_mode_card_res));
	ret_val = ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res);
	if (ret_val == -1) {
		errno_bak = errno;
		fprintf(stderr, "Failure 1: ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res); (%d)\n", errno_bak);
		fprintf(stderr, "errno: %s\n", strerror(errno_bak));
		_clean_up(fd, fbs, conn, &res, 0, 1, 1, 1);
		return 0;
	}
	
	res.fb_id_ptr = (uint64_t)calloc(res.count_fbs, sizeof(uint64_t));
	res.crtc_id_ptr = (uint64_t)calloc(res.count_crtcs, sizeof(uint64_t));
	res.connector_id_ptr = (uint64_t)calloc(res.count_connectors, sizeof(uint64_t));
	res.encoder_id_ptr = (uint64_t)calloc(res.count_encoders, sizeof(uint64_t));
	conn = calloc(res.count_connectors, sizeof(struct drm_mode_get_connector));
	fbs = calloc(res.count_connectors, sizeof(struct con_fb));
	if (res.fb_id_ptr == 0 || res.crtc_id_ptr == 0 || res.connector_id_ptr == 0 || res.encoder_id_ptr == 0 || conn == 0 || fbs == 0) {
		fprintf(stderr, "Error 1: calloc() failed to allocate.\n");
		_clean_up(fd, fbs, conn, &res, 0, 1, 1, 1);
		return 0;
	}
	
	ret_val = ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res);
	if (ret_val == -1) {
		errno_bak = errno;
		fprintf(stderr, "Failure 2: ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res); (%d)\n", errno_bak);
		fprintf(stderr, "errno: %s\n", strerror(errno_bak));
		_clean_up(fd, fbs, conn, &res, 0, 1, 1, 1);
		return 0;
	}
	unsigned int i;
	for (i = 0; i < res.count_connectors; i++) {
		uint64_t* cid_ptr = (uint64_t*)res.connector_id_ptr;
		conn[i].connector_id = cid_ptr[i];
		ret_val = ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, conn + i);
		if (ret_val == -1) {
			errno_bak = errno;
			fprintf(stderr, "[%d] Failure 1: ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, conn + i); (%d)\n", i, errno_bak);
			fprintf(stderr, "errno: %s\n", strerror(errno_bak));
			continue;
		}
		conn[i].encoders_ptr = (uint64_t)calloc(conn[i].count_encoders, sizeof(uint64_t));
		conn[i].modes_ptr = (uint64_t)calloc(conn[i].count_modes, sizeof(struct drm_mode_modeinfo));
		conn[i].props_ptr = (uint64_t)calloc(conn[i].count_props, sizeof(uint64_t));
		conn[i].prop_values_ptr = (uint64_t)calloc(conn[i].count_props, sizeof(uint64_t));
		if (conn[i].encoders_ptr == 0 || conn[i].modes_ptr == 0 || conn[i].props_ptr == 0 || conn[i].prop_values_ptr == 0) {
			fprintf(stderr, "[%d] Error 2: calloc() failed to allocate.\n", i);
			_clean_up(fd, fbs, conn, &res, i, 1, 1, 1);
			return 0;
		}
		ret_val = ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, conn + i);
		if (ret_val == -1) {
			errno_bak = errno;
			fprintf(stderr, "[%d] Failure 2: ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, conn + i); (%d)\n", i, errno_bak);
			fprintf(stderr, "errno: %s\n", strerror(errno_bak));
			_clean_up(fd, fbs, conn, &res, i, 1, 1, 1);
			return 0;
		}
		
		if (conn[i].count_encoders > 0 && conn[i].count_modes > 0 && conn[i].encoder_id != 0 && conn[i].connection != 0) {
			register struct drm_mode_modeinfo *mode_info;
			mode_info = (struct drm_mode_modeinfo*)conn[i].modes_ptr;
			
			unsigned int hdisplay;
			unsigned int vdisplay;
			
			struct drm_mode_create_dumb create_dumb_bfb;
			struct drm_mode_map_dumb map_dumb_bfb;
			struct drm_mode_fb_cmd cmd_dumb_bfb;
			struct drm_mode_create_dumb create_dumb_ffb;
			struct drm_mode_map_dumb map_dumb_ffb;
			struct drm_mode_fb_cmd cmd_dumb_ffb;
			struct drm_mode_get_encoder enc;
			struct drm_mode_crtc crtc;
			memset(&create_dumb_bfb, 0, sizeof(struct drm_mode_create_dumb));
			memset(&map_dumb_bfb, 0, sizeof(struct drm_mode_map_dumb));
			memset(&cmd_dumb_bfb, 0, sizeof(struct drm_mode_fb_cmd));
			memset(&create_dumb_ffb, 0, sizeof(struct drm_mode_create_dumb));
			memset(&map_dumb_ffb, 0, sizeof(struct drm_mode_map_dumb));
			memset(&cmd_dumb_ffb, 0, sizeof(struct drm_mode_fb_cmd));
			memset(&enc, 0, sizeof(struct drm_mode_get_encoder));
			memset(&crtc, 0, sizeof(struct drm_mode_crtc));
			
			create_dumb_bfb.width = mode_info[0].hdisplay;
			create_dumb_bfb.height = mode_info[0].vdisplay;
			create_dumb_bfb.bpp = 32;
			create_dumb_bfb.flags = 0;
			create_dumb_bfb.pitch = 0;
			create_dumb_bfb.size = 0;
			create_dumb_bfb.handle = 0;
			ret_val = ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb_bfb);
			if (ret_val == -1) {
				errno_bak = errno;
				fprintf(stderr, "[%d] Failure: ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb_bfb); (%d)\n", i, errno_bak);
				fprintf(stderr, "errno: %s\n", strerror(errno_bak));
				_clean_up(fd, fbs, conn, &res, i, 1, 1, 1);
				return 0;
			}
			fbs[i].bfb_db = create_dumb_bfb.handle;
			if (setup_doublebuffering == 1) {
				create_dumb_ffb.width = mode_info[0].hdisplay;
				create_dumb_ffb.height = mode_info[0].vdisplay;
				create_dumb_ffb.bpp = 32;
				create_dumb_ffb.flags = 0;
				create_dumb_ffb.pitch = 0;
				create_dumb_ffb.size = 0;
				create_dumb_ffb.handle = 0;
				ret_val = ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb_ffb);
				if (ret_val == -1) {
					errno_bak = errno;
					fprintf(stderr, "[%d] Failure: ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb_ffb); (%d)\n", i, errno_bak);
					fprintf(stderr, "errno: %s\n", strerror(errno_bak));
					_clean_up(fd, fbs, conn, &res, i, 1, 1, 1);
					return 0;
				}
				fbs[i].ffb_db = create_dumb_ffb.handle;
			} else {
				fbs[i].ffb_db = fbs[i].bfb_db;
			}
			
			cmd_dumb_bfb.width = create_dumb_bfb.width;
			cmd_dumb_bfb.height = create_dumb_bfb.height;
			cmd_dumb_bfb.bpp = create_dumb_bfb.bpp;
			cmd_dumb_bfb.pitch = create_dumb_bfb.pitch;
			cmd_dumb_bfb.depth = 24;
			cmd_dumb_bfb.handle = create_dumb_bfb.handle;
			ret_val = ioctl(fd, DRM_IOCTL_MODE_ADDFB, &cmd_dumb_bfb);
			if (ret_val == -1) {
				errno_bak = errno;
				fprintf(stderr, "[%d] Failure: ioctl(fd, DRM_IOCTL_MODE_ADDFB, &cmd_dumb_bfb); (%d)\n", i, errno_bak);
				fprintf(stderr, "errno: %s\n", strerror(errno_bak));
				_clean_up(fd, fbs, conn, &res, i, 1, 1, 1);
				return 0;
			}
			if (setup_doublebuffering == 1) {
				cmd_dumb_ffb.width = create_dumb_ffb.width;
				cmd_dumb_ffb.height = create_dumb_ffb.height;
				cmd_dumb_ffb.bpp = create_dumb_ffb.bpp;
				cmd_dumb_ffb.pitch = create_dumb_ffb.pitch;
				cmd_dumb_ffb.depth = 24;
				cmd_dumb_ffb.handle = create_dumb_ffb.handle;
				ret_val = ioctl(fd, DRM_IOCTL_MODE_ADDFB, &cmd_dumb_ffb);
				if (ret_val == -1) {
					errno_bak = errno;
					fprintf(stderr, "[%d] Failure: ioctl(fd, DRM_IOCTL_MODE_ADDFB, &cmd_dumb_ffb); (%d)\n", i, errno_bak);
					fprintf(stderr, "errno: %s\n", strerror(errno_bak));
					_clean_up(fd, fbs, conn, &res, i, 1, 1, 1);
					return 0;
				}
			}
			
			map_dumb_bfb.handle = create_dumb_bfb.handle;
			ret_val = ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb_bfb);
			if (ret_val == -1) {
				errno_bak = errno;
				fprintf(stderr, "[%d] Failure: ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb_bfb); (%d)\n", i, errno_bak);
				fprintf(stderr, "errno: %s\n", strerror(errno_bak));
				_clean_up(fd, fbs, conn, &res, i, 1, 1, 1);
				return 0;
			}
			if (setup_doublebuffering == 1) {
				map_dumb_ffb.handle = create_dumb_ffb.handle;
				ret_val = ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb_ffb);
				if (ret_val == -1) {
					errno_bak = errno;
					fprintf(stderr, "[%d] Failure: ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb_ffb); (%d)\n", i, errno_bak);
					fprintf(stderr, "errno: %s\n", strerror(errno_bak));
					_clean_up(fd, fbs, conn, &res, i, 1, 1, 1);
					return 0;
				}
			}
			
			fbs[i].fb_bfb = mmap(0, create_dumb_bfb.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map_dumb_bfb.offset);
			if (fbs[i].fb_bfb == 0 || fbs[i].fb_bfb == (void*)-1) {
				errno_bak = errno;
				fprintf(stderr, "[%d] bfb mmap() fail (%d)\n", i, errno_bak);
				fprintf(stderr, "errno: %s\n", strerror(errno_bak));
				fbs[i].fb_bfb = 0;
				_clean_up(fd, fbs, conn, &res, i, 1, 1, 1);
				return 0;
			}
			fbs[i].size = create_dumb_bfb.size;
			fbs[i].width = create_dumb_bfb.width;
			fbs[i].height = create_dumb_bfb.height;
			if (setup_doublebuffering == 1) {
				fbs[i].fb_ffb = mmap(0, create_dumb_ffb.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map_dumb_ffb.offset);
				if (fbs[i].fb_ffb == 0 || fbs[i].fb_ffb == (void*)-1) {
					errno_bak = errno;
					fprintf(stderr, "[%d] ffb mmap() fail (%d)\n", i, errno_bak);
					fprintf(stderr, "errno: %s\n", strerror(errno_bak));
					fbs[i].fb_ffb = 0;
					_clean_up(fd, fbs, conn, &res, i, 1, 1, 1);
					return 0;
				}
			} else {
				fbs[i].fb_ffb = fbs[i].fb_bfb;
			}
			
			enc.encoder_id = conn[i].encoder_id;
			ret_val = ioctl(fd, DRM_IOCTL_MODE_GETENCODER, &enc);
			if (ret_val == -1) {
				errno_bak = errno;
				fprintf(stderr, "[%d] Failure: ioctl(fd, DRM_IOCTL_MODE_GETENCODER, &enc); (%d)\n", i, errno_bak);
				fprintf(stderr, "errno: %s\n", strerror(errno_bak));
				_clean_up(fd, fbs, conn, &res, i, 1, 1, 1);
				return 0;
			}
			
			crtc.crtc_id = enc.crtc_id;
			ret_val = ioctl(fd, DRM_IOCTL_MODE_GETCRTC, &crtc);
			if (ret_val == -1) {
				errno_bak = errno;
				fprintf(stderr, "[%d] Failure: ioctl(fd, DRM_IOCTL_MODE_GETCRTC, &crtc); (%d)\n", i, errno_bak);
				fprintf(stderr, "errno: %s\n", strerror(errno_bak));
				_clean_up(fd, fbs, conn, &res, i, 1, 1, 1);
				return 0;
			}
			
			uint64_t* tmp_con_ptr = (uint64_t*)res.connector_id_ptr;
			crtc.set_connectors_ptr = (uint64_t)(tmp_con_ptr + i);
			if (setup_doublebuffering == 1) {
				crtc.fb_id = cmd_dumb_ffb.fb_id;
			} else {
				crtc.fb_id = cmd_dumb_bfb.fb_id;
			}
			crtc.count_connectors = 1;
			crtc.mode = mode_info[0];
			crtc.mode_valid = 1;
			crtc.x = 0;
			crtc.y = 0;
			ret_val = ioctl(fd, DRM_IOCTL_MODE_SETCRTC, &crtc);
			if (ret_val == -1) {
				errno_bak = errno;
				fprintf(stderr, "[%d] Failure: ioctl(fd, DRM_IOCTL_MODE_SETCRTC, &crtc); (%d)\n", i, errno_bak);
				fprintf(stderr, "errno: %s\n", strerror(errno_bak));
				_clean_up(fd, fbs, conn, &res, i, 1, 1, 1);
				return 0;
			}
			
			fbs[i].crtc_id = enc.crtc_id;
			fbs[i].bfb_id = cmd_dumb_bfb.fb_id;
			if (setup_doublebuffering == 1) {
				fbs[i].ffb_id = cmd_dumb_ffb.fb_id;
			} else {
				fbs[i].ffb_id = fbs[i].bfb_id;
			}
		} else {
			fprintf(stderr, "Connector: %d is Not Connected.\n", i);
		}
	}
	
	struct con_fb* fbs2 = 0;
	unsigned int l;
	unsigned int m;
	unsigned int n;
	m = 0;
	for (l = 0; l < i; l++) {
		if (fbs[l].fb_ffb != 0) {
			m++;
		}
	}
	if (m > 0) {
		fbs2 = calloc(m, sizeof(struct con_fb));
		if (fbs2 == 0) {
			fprintf(stderr, "calloc([%d], sizeof(struct con_fb));\n", m);
			_clean_up(fd, fbs, conn, &res, i, 1, 1, 1);
			return 0;
		}
	} else {
		_clean_up(fd, fbs, conn, &res, i, 1, 1, 1);
		return 0;
	}
	n = 0;
	for (l = 0; l < i; l++) {
		if (fbs[l].fb_ffb != 0) {
			fbs2[n] = fbs[l];
			fbs2[n].a_length = m;
			fbs2[n].line_length = fbs2[n].size / fbs2[n].height;
			fbs2[n].type = 1;
			n++;
		}
	}
	_clean_up(fd, fbs, conn, &res, i, 0, 0, 1);
	return fbs2;
}

struct con_fb* try_legacy_fb(signed int fd, unsigned int setup_doublebuffering) {
	unsigned int ret_val = 0;
	unsigned int errno_bak = 0;
	
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	
	ret_val = ioctl(fd, FBIOGET_FSCREENINFO, &finfo);
	if (ret_val == -1) {
		errno_bak = errno;
		fprintf(stderr, "Error reading fixed information.\n");
		fprintf(stderr, "errno: %s\n", strerror(errno_bak));
		return 0;
	}
	ret_val = ioctl(fd, FBIOGET_VSCREENINFO, &vinfo);
	if (ret_val == -1) {
		errno_bak = errno;
		fprintf(stderr, "Error reading variable information.  Stage-1\n");
		fprintf(stderr, "errno: %s\n", strerror(errno_bak));
		return 0;
	}
	if (vinfo.grayscale != 0 || vinfo.bits_per_pixel != 32 || vinfo.xoffset != 0 || vinfo.yoffset != 0) {
		vinfo.grayscale = 0;
		vinfo.bits_per_pixel = 32;
		vinfo.xoffset = 0;
		vinfo.yoffset = 0;
		ret_val = ioctl(fd, FBIOPUT_VSCREENINFO, &vinfo);
		if (ret_val == -1) {
			errno_bak = errno;
			fprintf(stderr, "Error setting variable information.  Stage-1\n");
			fprintf(stderr, "errno: %s\n", strerror(errno_bak));
			return 0;
		}
		ret_val = ioctl(fd, FBIOGET_VSCREENINFO, &vinfo);
		if (ret_val == -1) {
			errno_bak = errno;
			fprintf(stderr, "Error reading variable information.  Stage-2\n");
			fprintf(stderr, "errno: %s\n", strerror(errno_bak));
			return 0;
		}
		if (vinfo.grayscale != 0 || vinfo.bits_per_pixel != 32 || vinfo.xoffset != 0 || vinfo.yoffset != 0) {
			fprintf(stderr, "Error: Could not set modes.  Upon rechecking we found: \n");
			fprintf(stderr, "\tvinfo.grayscale == %d\n", vinfo.grayscale);
			fprintf(stderr, "\tvinfo.bits_per_pixel == %d\n", vinfo.bits_per_pixel);
			fprintf(stderr, "\tvinfo.xoffset == %d\n", vinfo.xoffset);
			fprintf(stderr, "\tvinfo.yoffset == %d\n", vinfo.yoffset);
			return 0;
		}
	}
	if (setup_doublebuffering == 1) {
		if (vinfo.yres * 2 > vinfo.yres_virtual) {
			vinfo.yres_virtual = vinfo.yres * 2;
			ret_val = ioctl(fd, FBIOPUT_VSCREENINFO, &vinfo);
			if (ret_val == -1) {
				errno_bak = errno;
				fprintf(stderr, "Error setting variable information.  Stage-2\n");
				fprintf(stderr, "errno: %s\n", strerror(errno_bak));
				return 0;
			}
			ret_val = ioctl(fd, FBIOGET_VSCREENINFO, &vinfo);
			if (ret_val == -1) {
				errno_bak = errno;
				fprintf(stderr, "Error reading variable information.  Stage-3\n");
				fprintf(stderr, "errno: %s\n", strerror(errno_bak));
				return 0;
			}
			if (vinfo.yres * 2 > vinfo.yres_virtual) {
				fprintf(stderr, "Error: Could not set modes.  Upon rechecking we found: \n");
				fprintf(stderr, "\tvinfo.yres_virtual < vinfo.yres * 2\n");
				fprintf(stderr, "\tvinfo.yres_virtual == %d\n", vinfo.yres_virtual);
				return 0;
			}
		}
	}
	
	errno = 0;
	void* ptr = 0;
	unsigned int screensize = vinfo.yres * finfo.line_length;
	if (setup_doublebuffering == 1) {
		ptr = mmap(ptr, screensize * 2, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	} else {
		ptr = mmap(ptr, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	}
	if (ptr == MAP_FAILED || ptr == 0 || errno != 0) {
		errno_bak = errno;
		fprintf(stderr, "Error: mmap() Failed!  Could not create virtual address space mapping!\n");
		fprintf(stderr, "errno: %s\n", strerror(errno_bak));
		return 0;
	}
	
	struct con_fb* fbs = 0;
	fbs = calloc(1, sizeof(struct con_fb));
	if (fbs == 0) {
		fprintf(stderr, "Error: calloc() Failed!  Could not allocate memory!\n");
		errno = 0;
		if (setup_doublebuffering == 1) {
			ret_val = munmap(ptr, screensize * 2);
		} else {
			ret_val = munmap(ptr, screensize);
		}
		if (ret_val == -1) {
			errno_bak = errno;
			fprintf(stderr, "Error: munmap() Failed!  Could not free virtual address space mapping!\n");
			fprintf(stderr, "errno: %s\n", strerror(errno_bak));
		}
		return 0;
	}
	
	fbs[0].line_length = finfo.line_length;
	fbs[0].width = vinfo.xres;
	fbs[0].height = vinfo.yres;
	fbs[0].size = screensize;
	fbs[0].a_length = 1;
	fbs[0].type = 2;
	fbs[0].fb_ffb = ptr;
	fbs[0].ffb_db = 0;
	fbs[0].bfb_db = vinfo.yres;
	if (setup_doublebuffering == 1) {
		fbs[0].fb_bfb = ptr + screensize;
		fbs[0].size = fbs[0].size * 2;
	} else {
		fbs[0].fb_bfb = ptr;
	}
	
	return fbs;
}
