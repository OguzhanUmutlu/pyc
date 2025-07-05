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
#include <ctype.h>
#include <math.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gmp.h>
#include <stdarg.h>

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

void sstream_appendf(sstream *a, const char *fmt, ...);

static inline void sstream_free(sstream *a) {
    if (a == NULL) return;
    if (a->data != NULL) free(a->data);
    a->data = NULL;
    a->size = 0;
}

vec_define_pyc(string, strings, char *);

vec_define(double, doubles);

typedef struct {
    size_t size, capacity;
    mpz_t *data;
} bigints;

static inline void bigints_init_reserved(bigints *v, size_t reserved) {
    v->size = 0;
    v->capacity = reserved;
    if (!(v->data = (mpz_t *) malloc(sizeof(mpz_t) * v->capacity))) {
        perror("malloc failed");
        raise(SIGSEGV);
        exit(EXIT_FAILURE);
    }
}

static inline void bigints_init(bigints *v) { bigints_init_reserved(v, 4); }

static inline void bigints_realloc(bigints *v, size_t n) {
    if (n == v->capacity)return;
    if (n == 0) {
        free(v->data);
        v->data = NULL;
    } else if (v->data == NULL) {
        v->data = (mpz_t *) malloc(sizeof(mpz_t) * n);
        if (!v->data) {
            perror("malloc failed");
            raise(SIGSEGV);
            exit(EXIT_FAILURE);
        }
    } else {
        mpz_t *newData = realloc(v->data, sizeof(mpz_t) * n);
        if (!newData) {
            perror("realloc failed");
            raise(SIGSEGV);
            exit(EXIT_FAILURE);
        }
        v->data = newData;
    }
    v->capacity = n;
    if (v->size > n) v->size = n;
}

static inline void bigints_reserve_push(bigints *v) {
    if (v->size >= v->capacity)
        bigints_realloc(v, v->capacity > 1 ? (v->capacity + (v->capacity >> 1)) : 2);
}

static inline bool bigints_empty(bigints v) { return v.size == 0; }

static inline void bigints_shrink(bigints *v) { if (v->size != v->capacity) bigints_realloc(v, v->size); }

static inline void bigints_clear(bigints *v) {
    if (!v->data) return;
    for (size_t i = 0; i < v->size; i++) {
        mpz_clear(v->data[i]);
    }
    free(v->data);
    v->data = NULL;
    v->size = 0;
    v->capacity = 0;
}

static inline void bigints_resize(bigints *v, size_t n, mpz_t def_val) {
    if (n > v->capacity) {
        for (size_t i = n; i < v->size; i++) {
            mpz_clear(v->data[i]);
        }
        bigints_realloc(v, n);
        while (v->size < n) { mpz_set(v->data[v->size++], def_val); }
    }
    v->size = n;
};

static inline void bigints_print_indent(bigints v, size_t indent) {
    if (!v.data) {
        printf("mpz_t[]");
        return;
    }
    printf("mpz_t[");
    if (v.size == 0) {
        printf("]");
        return;
    }
    putchar('\n');
    indent++;
    for (size_t i = 0; i < v.size; i++) {
        for (size_t j = 0; j < (indent) * 2; j++)
            putchar(' ');
        gmp_printf("%Zd", v.data[i]);
        if (i < v.size - 1) putchar(',');
        putchar('\n');
    }
    for (size_t i = 0; i < (indent <= 1 ? 0 : indent - 2) * 2; i++)putchar(' ');
    putchar(']');
};

#define ensure_str(x) ((x) == NULL ? "" : (x))

#define sstream_append(a, b) sstream_append_l(a, b, strlen(b))

#endif // PYC_UTILS_H
