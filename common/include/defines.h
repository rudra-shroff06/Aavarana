#ifndef DEFINES_H
#define DEFINES_H
#include <assert.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

static_assert(sizeof(char) * 8 == 8, "[Error]: char must be 8 bits!");
static_assert(sizeof(short) * 8 == 16, "[Error]: short must be 16 bits!");
static_assert(sizeof(int) * 8 == 32, "[Error]: int must be 32 bits!");
static_assert(sizeof(long) * 8 == 64, "[Error]: long must be 64 bits!");
static_assert(sizeof(float) * 8 == 32, "[Error]: float must be 32 bits!");
static_assert(sizeof(double) * 8 == 64, "[Error]: double must be 64 bits!");

typedef unsigned char u8;
typedef char i8;
typedef unsigned short u16;
typedef short i16;
typedef unsigned int u32;
typedef int i32;
typedef unsigned long u64;
typedef long i64;
typedef float f32;
typedef double f64;


#endif // DEFINES_H