#ifndef OBJECT_H
#define OBJECT_H

#include "common.h"

struct sType;

typedef struct sObjectHeader ObjectHeader;
typedef struct sObject Object;

struct sObjectHeader {
  unsigned int marked;
  struct sType* type;
  Object* next;
};

struct sObject {
  ObjectHeader header;
  char data[0];
};

#endif
