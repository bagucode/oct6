#ifndef VTABLE_H
#define VTABLE_H

#include "Common.h"

struct sType;

typedef struct sVTable VTable;

struct sVTable {
  struct sType* type;
  U8 functions[0];
};

#endif
