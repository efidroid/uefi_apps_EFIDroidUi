/********************************************************************[libaroma]*
 * Copyright (C) 2011-2015 Ahmad Amarullah (http://amarullz.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *______________________________________________________________________________
 *
 * Filename    : aroma_platform.h
 * Description : platform header
 *
 * + This is part of libaroma, an embedded ui toolkit.
 * + 06/04/15 - Author(s): Ahmad Amarullah
 *
 */
#ifndef __libaroma_aroma_internal_h__
  #error "Include <aroma_internal.h> instead."
#endif
#ifndef __libaroma_platform_h__
#define __libaroma_platform_h__

#include <unistd.h>     /* open, close, unlink, usleep */

#define LIBAROMA_CONFIG_OS "uefi/edk2"

/*
 * common platform wrapper
 */
#define libaroma_unlink(filename) unlink(filename)
#define libaroma_sleep(ms) usleep(ms*1000)

/*
 * get tick count
 */
static inline long libaroma_tick(){
  return 0;
}

/*
 * MUTEX - NEED MULTICORE THREAD SAFE
 */
#define LIBAROMA_MUTEX int
#define libaroma_mutex_init(x) (void)(x)
#define libaroma_mutex_free(x) (void)(x)
#define libaroma_mutex_lock(x) (void)(x)
#define libaroma_mutex_unlock(x) (void)(x)

/*
 * File Management
 */
int libaroma_filesize(const char * filename);
int libaroma_filesize_fd(int fd);
byte libaroma_file_exists(const char * filename);

//extern float _strtof(const char *s, char **sp);
//#define strtof        _strtof
double	round(double);

#endif /* __libaroma_platform_h__ */

