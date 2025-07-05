#include "compiler.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: pyc <filename>\n");
        return 1;
    }

    pyc_compile(argv[1], "pyc_out.c");
    return 0;
}
