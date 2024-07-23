
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

void run_file(char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) return;
    fseek(fp, 0, SEEK_END);
    int len = ftell(fp);
    char* program = malloc(len + 1);
    program[len - 1] = '\0';
    fseek(fp, 0, SEEK_SET);
    (void)! fread(program, 1, len, fp);
    fclose(fp);

    interpret(program);

    free(program);
}

int main(int argc, char** argv) {

    VM_init();

    if (argc < 2) {
        repl();
    } else {
        run_file(argv[1]);
    }

    VM_free();
    return 0;
}