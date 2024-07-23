#ifndef BUILTINS_H
#define BUILTINS_H

#include "value.h"

#define DECL_BUILTIN(name) int builtin_##name(int argc, Value* argv)
#define ADD_BUILTIN(name)                                                      \
    table_set(&vm.globals, CREATE_STRING_LITERAL(#name),                       \
              BUILTIN_VAL(builtin_##name))

DECL_BUILTIN(clock);
DECL_BUILTIN(print);
DECL_BUILTIN(println);
DECL_BUILTIN(scanln);

#endif
