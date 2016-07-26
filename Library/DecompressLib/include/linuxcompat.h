#ifndef LINUXCOMPAT_H
#define LINUXCOMPAT_H

// UEFI includes
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>

// libc includes
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// types
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef u16 __le16;
typedef u16 __be16;
typedef u32 __le32;
typedef u32 __be32;
typedef u64 __le64;
typedef u64 __be64;

// memory allocation
#define kmalloc(x, y) malloc(x)
#define kfree(x) free(x)
#define vmalloc(x) malloc(x)
#define vfree(x) free(x)

// debug
#define pr_debug(fmt, ...) DEBUG((EFI_D_INFO, fmt, __VA_ARGS__))

// attributes
#define __initconst
#define __init
#define noinline
#define __force
#define __always_inline	inline __attribute__((always_inline))

// module info
#define EXPORT_SYMBOL(x)
#define EXPORT_SYMBOL_GPL(x)
#define MODULE_LICENSE(x)
#define MODULE_DESCRIPTION(x)
#define MODULE_VERSION(x)
#define MODULE_AUTHOR(x)

// compiletime checks
#define unlikely(value) __builtin_expect((value), 0)
#define likely(value) (value)
#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))

#define min_t(type, x, y) ({			\
	type __min1 = (x);			\
	type __min2 = (y);			\
	__min1 < __min2 ? __min1: __min2; })

#define min(x, y) ({				\
	typeof(x) _min1 = (x);			\
	typeof(y) _min2 = (y);			\
	(void) (&_min1 == &_min2);		\
	_min1 < _min2 ? _min1 : _min2; })

#if defined(__LITTLE_ENDIAN)
static inline u32 le32_to_cpup(const __le32 *p)
{
	return (__force u32)*p;
}
#else
# error need to define endianess
#endif

#if defined(__LITTLE_ENDIAN)
# ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
#  include <linux/unaligned/le_struct.h>
#  include <linux/unaligned/be_byteshift.h>
# endif
# include <linux/unaligned/generic.h>
# define get_unaligned	__get_unaligned_le
# define put_unaligned	__put_unaligned_le
#elif defined(__BIG_ENDIAN)
# ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
#  include <linux/unaligned/be_struct.h>
#  include <linux/unaligned/le_byteshift.h>
# endif
# include <linux/unaligned/generic.h>
# define get_unaligned	__get_unaligned_be
# define put_unaligned	__put_unaligned_be
#else
# error need to define endianess
#endif

#endif // LINUXCOMPAT_H
