#ifndef COMMON_H
#define COMMON_H

#ifdef WIN32
#pragma warning (disable : 4996) // Stop whining about deprecated functions
#endif

#ifdef _DEBUG
#ifndef DEBUG
#define DEBUG
#endif
#elif defined NDEBUG
#ifndef RELEASE
#define RELEASE
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <setjmp.h>

#endif

