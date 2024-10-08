
#include <stdio.h>
#include <stdlib.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "chunk.h"
#include "vm.h"

#define USE_READLINE

void repl() {
    using_history();
    char* buf;
    printf("CLox\n");
    while (true) {
#ifdef USE_READLINE
        buf = readline("clox> ");
        if (!buf) break;
        add_history(buf);
        interpret(buf);
        free(buf);
#else
        buf = malloc(1000);
        printf("clox> ");
        if (!fgets(buf, 1000, stdin)) {
            free(buf);
            break;
        }
        interpret(buf);
        free(buf);
#endif
    }
}

int run_file(char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) return NO_FILE;
    fseek(fp, 0, SEEK_END);
    int len = ftell(fp);
    char* program = malloc(len + 1);
    program[len] = '\0';
    fseek(fp, 0, SEEK_SET);
    (void) !fread(program, 1, len, fp);
    fclose(fp);

    int code = interpret(program);

    free(program);
    return code;
}

int main(int argc, char** argv) {

    VM_init();
    atexit(VM_free);

    int exitcode = 0;
    if (argc < 2) {
        repl();
    } else {
        exitcode = run_file(argv[1]);
        if (exitcode == NO_FILE) perror("clox");
    }

    return exitcode;
}