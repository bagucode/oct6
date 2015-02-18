#ifndef COMMON_H
#define COMMON_H

#ifdef _DEBUG
# ifndef DEBUG
#  define DEBUG
# endif
#elif defined NDEBUG
# ifndef RELEASE
#  define RELEASE
# endif
#endif

#ifndef WIN32
# include <inttypes.h>
#else
#pragma warning (disable : 4996) // Stop whining about deprecated functions
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef signed __int64 int64_t;
typedef unsigned __int64 uint64_t;
#endif

#if defined(_WIN64) || defined(__LP64__) || defined(_M_AMD64) || defined(_M_X64)
# define OCT64
#else
# define OCT32
#endif

typedef int8_t   I8;
typedef uint8_t  U8;
typedef int16_t  I16;
typedef uint16_t U16;
typedef int32_t  I32;
typedef uint32_t U32;
typedef int64_t  I64;
typedef uint64_t U64;
typedef float    F32;
typedef double   F64;

#ifdef OCT64
typedef int64_t  Word;
typedef uint64_t Uword;
#else
typedef int32_t  Word;
typedef uint32_t Uword;
#endif

typedef void* Address;

typedef I8 Bool;
#define True 1
#define False 0

typedef int32_t Char;

#ifndef NULL
# define NULL 0
#endif

#endif

