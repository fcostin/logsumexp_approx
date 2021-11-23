#ifndef _LSEA_TYPES
#define _LSEA_TYPES 1
typedef struct {
    int offset;
    int width;
} range_t;


// double *data, range_t *ranges, int n_ranges -> double result
typedef double (*reduction_func_t)(double *, range_t *, int);


typedef struct {
    reduction_func_t f;
    void *m;
    size_t size;
} jit_reduction_func_t;

#endif
