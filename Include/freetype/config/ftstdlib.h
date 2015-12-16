/***************************************************************************/
/*                                                                         */
/*  ftstdlib.h                                                             */
/*                                                                         */
/*    ANSI-specific library and header configuration file (specification   */
/*    only).                                                               */
/*                                                                         */
/*  Copyright 2002-2015 by                                                 */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/


  /*************************************************************************/
  /*                                                                       */
  /* This file is used to group all #includes to the ANSI C library that   */
  /* FreeType normally requires.  It also defines macros to rename the     */
  /* standard functions within the FreeType source code.                   */
  /*                                                                       */
  /* Load a file which defines __FTSTDLIB_H__ before this one to override  */
  /* it.                                                                   */
  /*                                                                       */
  /*************************************************************************/


#ifndef __FTSTDLIB_H__
#define __FTSTDLIB_H__


typedef UINTN size_t;

#define ft_ptrdiff_t  ptrdiff_t


  /**********************************************************************/
  /*                                                                    */
  /*                           integer limits                           */
  /*                                                                    */
  /* UINT_MAX and ULONG_MAX are used to automatically compute the size  */
  /* of `int' and `long' in bytes at compile-time.  So far, this works  */
  /* for all platforms the library has been tested on.                  */
  /*                                                                    */
  /* Note that on the extremely rare platforms that do not provide      */
  /* integer types that are _exactly_ 16 and 32 bits wide (e.g. some    */
  /* old Crays where `int' is 36 bits), we do not make any guarantee    */
  /* about the correct behaviour of FT2 with all fonts.                 */
  /*                                                                    */
  /* In these case, `ftconfig.h' will refuse to compile anyway with a   */
  /* message like `couldn't find 32-bit type' or something similar.     */
  /*                                                                    */
  /**********************************************************************/


#ifdef __arm__
#define __CHAR_BIT        8
#define __SHRT_MAX        0x7fff
#define __USHRT_MAX       0xffff
#define __INT_MAX         0x7fffffff
#define __INT_MIN         (-0x7fffffff-1)
#define __UINT_MAX        0xffffffffU
#define __LONG_MAX        0x7fffffffL
#define __ULONG_MAX       0xffffffffUL
#endif

#define CHAR_BIT    __CHAR_BIT
#define SHRT_MAX    __SHRT_MAX
#define USHRT_MAX   __USHRT_MAX
#define INT_MAX     __INT_MAX
#define INT_MIN     __INT_MIN
#define UINT_MAX    __UINT_MAX
#define LONG_MAX    __LONG_MAX
#define ULONG_MAX   __ULONG_MAX

#define FT_CHAR_BIT    CHAR_BIT
#define FT_USHORT_MAX  USHRT_MAX
#define FT_INT_MAX     INT_MAX
#define FT_INT_MIN     INT_MIN
#define FT_UINT_MAX    UINT_MAX
#define FT_LONG_MAX    LONG_MAX
#define FT_ULONG_MAX   ULONG_MAX


  /**********************************************************************/
  /*                                                                    */
  /*                 character and string processing                    */
  /*                                                                    */
  /**********************************************************************/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>

/** The strrchr function locates the last occurrence of c (converted to a char)
    in the string pointed to by s. The terminating null character is considered
    to be part of the string.

    @return   The strrchr function returns a pointer to the character, or a
              null pointer if c does not occur in the string.
**/
static inline char *
strrchr(const char *s, int c)
{
  char  *found  = NULL;
  char  tgt     = (char)c;

  do {
    if( *s == tgt)  found = (char *)s;
  } while( *s++ != '\0');

  return found;
}

/** The memset function copies the value of c (converted to an unsigned char)
    into each of the first n characters of the object pointed to by s.

    @return   The memset function returns the value of s.
**/
static inline void *
memset(void *s, int c, size_t n)
{
  return SetMem( s, (UINTN)n, (UINT8)c);
}

/** The memchr function locates the first occurrence of c (converted to an
    unsigned char) in the initial n characters (each interpreted as
    unsigned char) of the object pointed to by s.

    @return   The memchr function returns a pointer to the located character,
              or a null pointer if the character does not occur in the object.
**/
static inline void *
memchr(const void *s, int c, size_t n)
{
  return ScanMem8( s, (UINTN)n, (UINT8)c);
}

/** The memcmp function compares the first n characters of the object pointed
    to by s1 to the first n characters of the object pointed to by s2.

    @return   The memcmp function returns an integer greater than, equal to, or
              less than zero, accordingly as the object pointed to by s1 is
              greater than, equal to, or less than the object pointed to by s2.
**/
static inline int
memcmp(const void *s1, const void *s2, size_t n)
{
  return (int)CompareMem( s1, s2, n);
}

#define ft_memchr   memchr
#define ft_memcmp   memcmp
#define ft_memcpy   CopyMem
#define ft_memmove  CopyMem
#define ft_memset   memset
#define ft_strcat   AsciiStrCat
#define ft_strcmp   AsciiStrCmp
#define ft_strcpy   AsciiStrCpy
#define ft_strlen   AsciiStrLen
#define ft_strncmp  AsciiStrnCmp
#define ft_strncpy  AsciiStrnCpy
#define ft_strrchr  strrchr
#define ft_strstr   AsciiStrStr
#define strlen      ft_strlen


  /**********************************************************************/
  /*                                                                    */
  /*                           file handling                            */
  /*                                                                    */
  /**********************************************************************/


#include <Library/PrintLib.h>

#define FT_FILE     FILE
#define ft_fclose   fclose
#define ft_fopen    fopen
#define ft_fread    fread
#define ft_fseek    fseek
#define ft_ftell    ftell
#define ft_sprintf(s, f, ...)  AsciiSPrint(s, UINT_MAX, f, __VA_ARGS__)


  /**********************************************************************/
  /*                                                                    */
  /*                             sorting                                */
  /*                                                                    */
  /**********************************************************************/


void qsort (void* base, size_t num, size_t size,
            int (*compar)(const void*, const void*));

#define ft_qsort  qsort


  /**********************************************************************/
  /*                                                                    */
  /*                        memory allocation                           */
  /*                                                                    */
  /**********************************************************************/


void * calloc(size_t Num, size_t Size);
void free(void *Ptr);
void * malloc(size_t Size);
void * realloc(void *Ptr, size_t ReqSize);

#define ft_scalloc   calloc
#define ft_sfree     free
#define ft_smalloc   malloc
#define ft_srealloc  realloc


  /**********************************************************************/
  /*                                                                    */
  /*                          miscellaneous                             */
  /*                                                                    */
  /**********************************************************************/


static inline int isspace(int c)
{
	return (c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v');
}

/** The atol function converts the initial portion of the string pointed to by
    nptr to long int representation.  Except for the behavior on error, it is
    equivalent to:
      - strtol(nptr, (char **)NULL, 10)

  @return   The atol function returns the converted value.
**/
static inline long int
efi_atol(const char *nptr)
{
  long int  Retval;
  BOOLEAN   Negative = FALSE;

  while(isspace(*nptr)) ++nptr; // Skip leading spaces

  if(*nptr == '+') {
    Negative = FALSE;
    ++nptr;
  }
  else if(*nptr == '-') {
    Negative = TRUE;
    ++nptr;
  }
  Retval = (long int)AsciiStrDecimalToUint64(nptr);
  if(Negative) {
    Retval = -Retval;
  }
  return Retval;
}

#define ft_atol  efi_atol


  /**********************************************************************/
  /*                                                                    */
  /*                         execution control                          */
  /*                                                                    */
  /**********************************************************************/


typedef BASE_LIBRARY_JUMP_BUFFER jmp_buf[1];

static inline void longjmp(jmp_buf env, int val)
{
  LongJump(env, (UINTN)((val == 0) ? 1 : val));
}

#define setjmp(env)   (INTN)SetJump((env))

#define ft_jmp_buf     jmp_buf  /* note: this cannot be a typedef since */
                                /*       jmp_buf is defined as a macro  */
                                /*       on certain platforms           */

#define ft_longjmp     longjmp
#define ft_setjmp( b ) setjmp( *(ft_jmp_buf*) &(b) ) /* same thing here */



#endif /* __FTSTDLIB_H__ */


/* END */
