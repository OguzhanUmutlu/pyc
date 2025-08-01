#ifndef PYC_UTILS_H
#define PYC_UTILS_H

// this should be defined if you want to use the production version
// #define PYC_PRODUCTION

#define fail()                                                                 \
    do {                                                                       \
        raise(SIGSEGV);                                                        \
        exit(EXIT_FAILURE);                                                    \
    } while (0)

#ifndef PYC_PRODUCTION
#define NEED_AST_PRINT
#endif

#ifdef NEED_AST_PRINT
#define vec_define_pyc(singular, plural, val)                              \
        void singular##_free(val obj);                                         \
        vec_define(val, plural);                                               \
        vec_define_free(val, plural, singular##_free(a));                      \
        void singular##_print(val obj, int indent);                            \
        vec_define_print(val, plural, singular##_print(a, indent));
#else
#define vec_define_pyc(singular, plural, val)                              \
        void singular##_free(val obj);                                         \
        vec_define(val, plural, plural);                                       \
        vec_define_free(val, plural, plural##_free, singular##_free);
#endif

#include "vec.h"
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define print_spaces(spaces)                                                   \
    for (int __ir = 0; __ir < spaces; __ir++)                                  \
    putchar(' ')

static inline char *read_file(char *filename) {
    FILE *file = fopen(filename, "r");

    if (file == NULL) {
        perror("Error opening file");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *code = (char *) malloc(file_size);
    if (code == NULL) {
        perror("Error allocating memory");
        fclose(file);
        return NULL;
    }

    fread(code, 1, file_size, file);

    size_t new_size = 0;
    for (int i = 0; i < file_size; i++) {
        if (code[i] != '\r')
            new_size++;
    }

    char *new_code = (char *) malloc(new_size + 1);
    if (new_code == NULL) {
        perror("Error allocating memory");
        free(code);
        fclose(file);
        return NULL;
    }

    for (size_t i = 0, j = 0; i < file_size; i++) {
        char c = code[i];
        if (c == '\0') {
            fprintf(stderr, "Error: Null character found in file\n");
            free(code);
            free(new_code);
            fclose(file);
            return NULL;
        }
        if (c != '\r')
            new_code[j++] = c;
    }
    free(code);
    new_code[new_size] = '\0';

    fclose(file);

    return new_code;
}

typedef struct {
    char *data;
    size_t size;
} sstream;

void sstream_print(sstream *a, int _);

void sstream_append_l(sstream *a, char *b, size_t b_len);

void sstream_appendf(sstream *a, char *fmt, ...);

static inline void sstream_free(sstream *a) {
    if (a == NULL) return;
    if (a->data != NULL) free(a->data);
    a->data = NULL;
    a->size = 0;
}

vec_define_pyc(string, strings, char *);

vec_define(uint64_t, u64_list);
vec_define(double, f64_list);

#define ensure_str(x) ((x) == NULL ? "" : (x))

#ifndef __THROWNL
#define __THROWNL
#endif

#endif // PYC_UTILS_H
