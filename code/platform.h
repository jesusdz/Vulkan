/* date = September 10th 2020 0:27 am */

#ifndef PLATFORM_H
#define PLATFORM_H

typedef char                   i8;
typedef short int              i16;
typedef long int               i32;
typedef long long int          i64;

typedef unsigned char          u8;
typedef unsigned short int     u16;
typedef unsigned long  int     u32;
typedef unsigned long long int u64;

typedef unsigned long  int     b32;

typedef float                  f32;
typedef double                 f64;

#define U8_MAX                 255
#define U16_MAX                65535
#define U32_MAX                4294967295

#define LO_WORD(val) (0xFFFF &  val     )
#define HI_WORD(val) (0xFFFF & (val>>16))

#define IS_BIT_SET(flags, bit_idx) (1 & ((flags)>>bit_idx))

#define Assert(test) if (!(test)) *((volatile int*)0) = 0

#if 1 // Pre C++11 way of compile time asserts
#define CTAssert3(Expr, Line) struct CTAssert_____##Line {int Foo[(Expr) ? 1 : - 1];};
#define CTAssert2(Expr, Line) CTAssert3(Expr, Line)
#define CTAssert (Expr) CTAssert2(Expr, __LINE__)
#else
#define CTAssert (Expr) static_assert(Expr, "Assertion failed: "#Expr)
#endif

#define NotImplemented Assert(!"Not implemented")

#define KB(n) (  (n)*1024)
#define MB(n) (KB(n)*1024)
#define GB(n) (MB(n)*1024)
#define TB(n) (GB(n)*1024)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#define OffsetOf(Type, Member) ((u32)&(((Type*)0)->Member))

#define Max(a, b) (((a)>(b))?(a):(a))
#define Min(a, b) (((a)<(b))?(a):(a))

#define internal static

#define local_persist static

#endif //PLATFORM_H
