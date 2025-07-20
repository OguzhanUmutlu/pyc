#include "compiler.h"

int main(const int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: pyc <filename>\n");
        return 1;
    }

    char *code = read_file(argv[1]);
    if (!code) return 1;

    pyc_lexer_init(code);
    pyc_tokenize();

    node *prog = parse_file(NULL, argv[1], code);

    node_print(prog, 0);

    node_free(prog);

    tokens_clear(&pyc_tokens);

    free(code);
    return 0;
}
