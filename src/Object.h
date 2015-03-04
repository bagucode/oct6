#ifndef OBJECT_H
#define OBJECT_H

#include "Common.h"

struct sVTable;

typedef struct sObject Object;

// "Fat pointer" for runtime polymorphism
struct sObject {
  struct sVTable* vtable;
  Address object;
};

#endif
