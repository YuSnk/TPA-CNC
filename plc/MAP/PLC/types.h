#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef unsigned char       BYTE;
typedef unsigned short int  WORD;
typedef unsigned int        UINT;
typedef unsigned short int  REG;

#define ZeroMemory(ptr, size) memset((ptr), 0, (size))

#endif
