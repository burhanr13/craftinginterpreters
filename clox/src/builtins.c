#include "builtins.h"

#include <time.h>

#include "compiler.h"
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

DECL_BUILTIN(getc) {
    argv[0] = CHAR_VAL(getc(stdin));
    return OK;
}

DECL_BUILTIN(loadModule) {
    if (argc < 1) return RUNTIME_ERROR;
    if (!isObjType(argv[1], OT_STRING)) return RUNTIME_ERROR;

    ObjString* filename = (ObjString*) argv[1].obj;

    FILE* fp = fopen(filename->data, "r");
    if (!fp) return RUNTIME_ERROR;
    fseek(fp, 0, SEEK_END);
    int len = ftell(fp);
    char* program = malloc(len + 1);
    program[len] = '\0';
    fseek(fp, 0, SEEK_SET);
    (void) !fread(program, 1, len, fp);
    fclose(fp);

    gc_disable();
    ObjFunction* compiled = compile(program);
    free(program);

    if (!compiled) return RUNTIME_ERROR;

    gc_enable();
    argv[0] = OBJ_VAL(compiled);
    return OK;
}
