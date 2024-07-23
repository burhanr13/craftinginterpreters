#include "builtins.h"

#include <time.h>

#include "object.h"
#include "value.h"
#include "vm.h"

DECL_BUILTIN(clock) {
    argv[0] = NUMBER_VAL((double) clock() / CLOCKS_PER_SEC);
    return OK;
}

DECL_BUILTIN(print) {
    if (argc > 0) {
        print_value(argv[1]);
    }
    argv[0] = NIL_VAL;
    return OK;
}

DECL_BUILTIN(println) {
    if (argc > 0) {
        print_value(argv[1]);
    }
    printf("\n");
    argv[0] = NIL_VAL;
    return OK;
}

DECL_BUILTIN(scanln) {
    Vector(char) str;
    Vec_init(str);
    char c;
    while ((c = getc(stdin)) != '\n') Vec_push(str, c);
    ObjString* o = create_string(str.d, str.size);
    Vec_free(str);
    argv[0] = OBJ_VAL(o);
    return OK;
}
