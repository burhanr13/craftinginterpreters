#ifndef COMPILER_H
#define COMPILER_H

#include "chunk.h"
#include "object.h"
#include "types.h"

ObjFunction* compile(char* source);

#endif