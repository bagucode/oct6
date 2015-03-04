#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "Common.h"

struct sType;
struct sObject;

typedef struct sProtocol Protocol;

struct sProtocol {
	struct sType* type;
	struct sObject* object;
};

#endif

