#include <aroma.h>
#include <math.h>

static LIBAROMA_CONFIG _libaroma_config;
static byte _libaroma_config_ready=0;
static FILE * _libaroma_debug_fp=NULL;
static char _libaroma_debug_tag[256]="LIBAROMA()";

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
  _libaroma_debug_fp=stdout;
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

/*
 * Function    : libaroma_debug_output
 * Return Value: FILE *
 * Descriptions: get debug output fd
 */
FILE * libaroma_debug_output(){
  if (!_libaroma_debug_fp){
    _libaroma_debug_fp=stdout;
  }
  return _libaroma_debug_fp;
} /* End of libaroma_debug_output */

/*
 * Function    : libaroma_debug_tag
 * Return Value: char *
 * Descriptions: get debug tag
 */
char * libaroma_debug_tag(){
  return _libaroma_debug_tag;
} /* End of libaroma_debug_tag */
