#include <aroma.h>
#include <math.h>

static LIBAROMA_CONFIG _libaroma_config;
static byte _libaroma_config_ready=0;

/*
 * Function    : _libaroma_config_default
 * Return Value: void
 * Descriptions: set default runtime configuration
 */
void _libaroma_config_default() {
  /*
  if (LIBAROMA_FB_SHMEM_NAME){
    snprintf(_libaroma_config.fb_shm_name,256,"%s",LIBAROMA_FB_SHMEM_NAME);
  }
  else{*/
    _libaroma_config.fb_shm_name[0]=0;
  /*}*/
  _libaroma_config.multicore_init_num = 8; /* activate core */
  _libaroma_config.snapshoot_fb = 0; /* snapshoot after graph init */
  _libaroma_config.runtime_monitor = LIBAROMA_START_UNSAFE;
  _libaroma_config_ready = 1;
} /* End of libaroma_config_default */

/*
 * Function    : libaroma_config
 * Return Value: LIBAROMA_CONFIGP
 * Descriptions: get runtime config
 */
LIBAROMA_CONFIGP libaroma_config(){
  if (!_libaroma_config_ready){
    _libaroma_config_default();
  }
  return &_libaroma_config;
} /* End of libaroma_config */

int libaroma_filesize(const char * filename) {
  return -1;
}
int libaroma_filesize_fd(int fd) {
  return -1;
}
byte libaroma_file_exists(const char * filename) {
  return 0;
}

double
round(double x)
{
	double t;

	if (!isfinite(x))
		return (x);

	if (x >= 0.0) {
		t = floor(x);
		if (t - x <= -0.5)
			t += 1.0;
		return (t);
	} else {
		t = floor(-x);
		if (t + x <= -0.5)
			t += 1.0;
		return (-t);
	}
}

float
roundf(float x)
{
	float t;

	if (!isfinite(x))
		return (x);

	if (x >= 0.0) {
		t = floorf(x);
		if (t - x <= -0.5)
			t += 1.0;
		return (t);
	} else {
		t = floorf(-x);
		if (t + x <= -0.5)
			t += 1.0;
		return (-t);
	}
}
