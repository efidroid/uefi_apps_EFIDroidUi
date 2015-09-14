#ifndef LINUXCOMPAT_H
#define LINUXCOMPAT_H

// UEFI includes
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>

// types
typedef UINT8 u8;
typedef UINT16 u16;
typedef UINT32 u32;
typedef UINT64 u64;
typedef UINTN size_t;
typedef u16 __le16;
typedef u16 __be16;
typedef u32 __le32;
typedef u32 __be32;
typedef u64 __le64;
typedef u64 __be64;

typedef UINT8 uint8_t;
typedef UINT16 uint16_t;
typedef UINT32 uint32_t;
typedef UINT64 uint64_t;
typedef INT32 int32_t;
typedef BOOLEAN bool;

#define false FALSE
#define true TRUE

// errno
#define	ENOMEM		12	/* Out of memory */
#define	EINVAL		22	/* Invalid argument */

// memory allocation
#define malloc(x) AllocatePool(x)
#define free(x) FreePool(x)
#define kmalloc(x, y) malloc(x)
#define kfree(x) free(x)
#define vmalloc(x) malloc(x)
#define vfree(x) free(x)

// memory operations
#define memcpy CopyMem
#define memcmp CompareMem
#define memset(buf, val, sz) SetMem(buf, sz, val)
#define memmove CopyMem
//#define memzero(s, n)     memset((s), (0), (n))

// debug
#define pr_debug(fmt, ...) DEBUG((EFI_D_INFO, fmt, __VA_ARGS__))

// attributes
#define __initconst
#define __init
#define noinline
#define __packed __attribute__((packed))
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
