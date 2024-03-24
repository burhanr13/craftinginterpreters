
#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "vm.h"

void repl() {
    char buf[1000];
    printf("CLox\n");
    while (true) {
        printf("clox> ");
        if (!fgets(buf, 1000, stdin)) break;
        interpret(buf);
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
    fread(program, 1, len, fp);
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