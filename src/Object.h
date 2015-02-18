#ifndef OBJECT_H
#define OBJECT_H

#include "Common.h"

struct sType;

typedef struct sObjectHeader ObjectHeader;
typedef struct sObject Object;

struct sObjectHeader {
  Bool marked;
  struct sType* type;
  Object* next;
};

struct sObject {
  ObjectHeader header;
  Address data[0];
};

#endif
