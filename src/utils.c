#include "utils.h"

void sstream_print(sstream *a, int _) {
    printf("%.*s", (int) a->size, a->data);
}

void sstream_append_l(sstream *a, char *b, size_t b_len) {
    size_t new_size = a->size + b_len;
    char *new_data = a->data == NULL ? malloc(new_size) : realloc(a->data, new_size);
    if (new_data == NULL) {
        perror("Error reallocating memory for sstream");
        exit(EXIT_FAILURE);
    }
    a->data = new_data;
    if (b != NULL) memcpy(new_data + a->size, b, b_len); // set b=NULL, if you want to manually set the data
    a->size = new_size;
}

void sstream_appendf(sstream *a, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (needed < 0) {
        va_end(args);
        perror("Error formatting string in sstream_appendf");
        exit(EXIT_FAILURE);
    }

    char *buf = malloc(needed + 1);
    if (buf == NULL) {
        va_end(args);
        perror("Error allocating temporary buffer in sstream_appendf");
        exit(EXIT_FAILURE);
    }

    vsnprintf(buf, needed + 1, fmt, args);
    va_end(args);

    sstream_append_l(a, buf, (size_t) needed);
    free(buf);
}