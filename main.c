#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "types.h"

#include "jit_logsumexp.c"

// ref: Curioni -- Fast Exponential Computation on SIMD Architectures
// ref: Schraudolph -- A Fast, Compact Approximation of the Exponential Function

#define APPROX_LN2 (0.6931471805599453)
// APPROX_S0 can be set to 0 if loading into a 32 bit int,
// as per Schraudolph, or 32 if loading into a 64 bit int.
#define APPROX_S0 (32l)
#define APPROX_S (1l << (20l + APPROX_S0))
#define APPROX_A (APPROX_S / APPROX_LN2)
#define APPROX_B (APPROX_S * 1023l)
#define APPROX_C (60801l * (1l << APPROX_S0))
#define APPROX_A_INV (1.0 / APPROX_A)

#define FAST_EXP_MIN_ARG -706.0


#define MODE_BASE 1
#define MODE_FAST 2
#define MODE_ONLY_SUM 3
#define MODE_FASTER 4
#define MODE_FASTERBB 6
#define MODE_JIT 7


static inline double reinterpret_long_as_double(long int x) {
    // type pun: reinterpret the bits of x as a 64 bit float.
    // this is expected to compile to a no-op.
    // with C++ we'd use "reinterpret cast".
    union {
        long int i;
        double d;
    } b;
    b.i = x;
    return b.d;
}


static inline long int reinterpret_double_as_long(double x) {
    // type pun: reinterpret the bits of x as a 64 bit int.
    // this is expected to compile to a no-op.
    // with C++ we'd use "reinterpret cast".
    union {
        long int i;
        double d;
    } b;
    b.d = x;
    return b.i;
}


double fast_exp(double x) {
    double z;
    z = reinterpret_long_as_double((long int)(fma(APPROX_A, x, + (APPROX_B - APPROX_C))));
    // above approximation gives bad results where x < -706.0
    return (x >= FAST_EXP_MIN_ARG) ? z : 0.0;
}


double fast_log(double x) {
    // precondition: x >= 0.0
    //
    // naively invert fast_exp
    // y = (a * x) + b
    // x = (y - b) / a
    // x = (1/a) * y + (1/a) * (-b)  // distribute multiply for fma
    double z;
    z = (double)reinterpret_double_as_long(x);
    z = fma(APPROX_A_INV, z, APPROX_A_INV * (- APPROX_B + APPROX_C));
    return (x > 0.0) ? z : -INFINITY;
}


double sum(double *a, int n) {
    // preconditions:
    // -inf <= a[i] <= 0.0 for all i = 0, ..., n-1
    double acc;
    int i;
    acc = 0.0;
    for (i = 0; i < n; ++i) {
        acc += a[i];
    }
    return acc;
}


double log_sum_exp(double *a, int n) {
    // preconditions:
    // -inf <= a[i] <= 0.0 for all i = 0, ..., n-1
    double a_max, acc;
    int i;
    a_max = -INFINITY;
    for (i = 0; i < n; ++i) {
        a_max = fmax(a[i], a_max);
    }
    if (a_max <= -INFINITY || n <= 1) {
        return a_max;
    }
    acc = 0.0;
    for (i = 0; i < n; ++i) {
        acc += exp(a[i] - a_max);
    }
    return log(acc) + a_max;
}


double fast_log_sum_exp(double *a, int n) {
    // preconditions:
    // -inf <= a[i] <= 0.0 for all i = 0, ..., n-1
    double a_max, acc;
    int i;
    a_max = -INFINITY;
    for (i = 0; i < n; ++i) {
        a_max = fmax(a[i], a_max);
    }
    if (a_max <= -INFINITY || n <= 1) {
        return a_max;
    }
    acc = 0.0;
    for (i = 0; i < n; ++i) {
        acc += fast_exp(a[i] - a_max);
    }
    return log(acc) + a_max;
}


double faster_log_sum_exp(double *a, int n) {
    // preconditions:
    // -inf <= a[i] <= 0.0 for all i = 0, ..., n-1
    double a_max, acc;
    int i;
    a_max = -INFINITY;
    for (i = 0; i < n; ++i) {
        a_max = fmax(a[i], a_max);
    }
    if (a_max <= -INFINITY || n <= 1) {
        return a_max;
    }
    // TODO: consider trick of biasing a_max to push more information into ieee exponent bits
    acc = 0.0;
    for (i = 0; i < n; ++i) {
        acc += fast_exp(a[i] - a_max);
    }
    return fast_log(acc) + a_max;
}


static inline double faster_log_sum_exp_1(const double *a) {
    return a[0];
}

static inline double faster_log_sum_exp_2(const double *a) {
    double a_max;
    a_max = fmax(a[0], a[1]);
    if (a_max <= -INFINITY) {
        return a_max;
    }
    return fast_log(
        fast_exp(a[0] - a_max) +
        fast_exp(a[1] - a_max)
    ) + a_max;
}

static inline double faster_log_sum_exp_3(const double *a) {
    double a_max;
    a_max = fmax(fmax(a[0], a[1]), a[2]);
    if (a_max <= -INFINITY) {
        return a_max;
    }
    return fast_log(
        fast_exp(a[0] - a_max) +
        fast_exp(a[1] - a_max) +
        fast_exp(a[2] - a_max)
    ) + a_max;
}

static inline double faster_log_sum_exp_4(const double *a) {
    double a_max;
    a_max = fmax(fmax(a[0], a[1]), fmax(a[2], a[3]));
    if (a_max <= -INFINITY) {
        return a_max;
    }
    return fast_log(
        fast_exp(a[0] - a_max) +
        fast_exp(a[1] - a_max) +
        fast_exp(a[2] - a_max) +
        fast_exp(a[3] - a_max)
    ) + a_max;
}

static inline double faster_log_sum_exp_5(const double *a) {
    double a_max;
    a_max = fmax(fmax(fmax(a[0], a[1]), fmax(a[2], a[3])), a[4]);
    if (a_max <= -INFINITY) {
        return a_max;
    }
    return fast_log(
        fast_exp(a[0] - a_max) +
        fast_exp(a[1] - a_max) +
        fast_exp(a[2] - a_max) +
        fast_exp(a[3] - a_max) +
        fast_exp(a[4] - a_max)
    ) + a_max;
}

static inline double faster_log_sum_exp_6(const double *a) {
    double a_max;
    a_max = fmax(fmax(fmax(a[0], a[1]), fmax(a[2], a[3])), fmax(a[4], a[5]));
    if (a_max <= -INFINITY) {
        return a_max;
    }
    return fast_log(
        fast_exp(a[0] - a_max) +
        fast_exp(a[1] - a_max) +
        fast_exp(a[2] - a_max) +
        fast_exp(a[3] - a_max) +
        fast_exp(a[4] - a_max) +
        fast_exp(a[5] - a_max)
    ) + a_max;
}

static inline double faster_log_sum_exp_7(const double *a) {
    double a_max;
    a_max = fmax(fmax(fmax(a[0], a[1]), fmax(a[2], a[3])), fmax(fmax(a[4], a[5]), a[6]));
    if (a_max <= -INFINITY) {
        return a_max;
    }
    return fast_log(
        fast_exp(a[0] - a_max) +
        fast_exp(a[1] - a_max) +
        fast_exp(a[2] - a_max) +
        fast_exp(a[3] - a_max) +
        fast_exp(a[4] - a_max) +
        fast_exp(a[5] - a_max) +
        fast_exp(a[6] - a_max)
    ) + a_max;
}

static inline double faster_log_sum_exp_8(const double *a) {
    double a_max;
    a_max = fmax(fmax(fmax(a[0], a[1]), fmax(a[2], a[3])), fmax(fmax(a[4], a[5]), fmax(a[6], a[7])));
    if (a_max <= -INFINITY) {
        return a_max;
    }
    return fast_log(
        fast_exp(a[0] - a_max) +
        fast_exp(a[1] - a_max) +
        fast_exp(a[2] - a_max) +
        fast_exp(a[3] - a_max) +
        fast_exp(a[4] - a_max) +
        fast_exp(a[5] - a_max) +
        fast_exp(a[6] - a_max) +
        fast_exp(a[7] - a_max)
    ) + a_max;
}

static inline double faster_log_sum_exp_9(const double *a) {
    double a_max;
    a_max = fmax(fmax(fmax(fmax(a[0], a[1]), fmax(a[2], a[3])), fmax(fmax(a[4], a[5]), fmax(a[6], a[7]))), a[8]);
    if (a_max <= -INFINITY) {
        return a_max;
    }
    return fast_log(
        fast_exp(a[0] - a_max) +
        fast_exp(a[1] - a_max) +
        fast_exp(a[2] - a_max) +
        fast_exp(a[3] - a_max) +
        fast_exp(a[4] - a_max) +
        fast_exp(a[5] - a_max) +
        fast_exp(a[6] - a_max) +
        fast_exp(a[7] - a_max) +
        fast_exp(a[8] - a_max)
    ) + a_max;
}

static inline double faster_log_sum_exp_10(const double *a) {
    double a_max;
    a_max = fmax(fmax(fmax(fmax(a[0], a[1]), fmax(a[2], a[3])), fmax(fmax(a[4], a[5]), fmax(a[6], a[7]))), fmax(a[8], a[9]));
    if (a_max <= -INFINITY) {
        return a_max;
    }
    return fast_log(
        fast_exp(a[0] - a_max) +
        fast_exp(a[1] - a_max) +
        fast_exp(a[2] - a_max) +
        fast_exp(a[3] - a_max) +
        fast_exp(a[4] - a_max) +
        fast_exp(a[5] - a_max) +
        fast_exp(a[6] - a_max) +
        fast_exp(a[7] - a_max) +
        fast_exp(a[8] - a_max) +
        fast_exp(a[9] - a_max)
    ) + a_max;
}


double faster_log_sum_exp_bb(range_t *ranges, double *logps, int n) {
    // this version only supports ranges of with 1 -- 10.
    // pre-req: input ranges ordered with nondecreasing width

    // method       running time (s)
    // ------
    // faster       0.532
    // fasterbb     0.421

    double acc = 0.0;

    int i = 0.0;

    for(; i < n && ranges[i].width == 1; ++i) {
        acc += faster_log_sum_exp_1(&(logps[ranges[i].offset]));
    }
    for(; i < n && ranges[i].width == 2; ++i) {
        acc += faster_log_sum_exp_2(&(logps[ranges[i].offset]));
    }
    for(; i < n && ranges[i].width == 3; ++i) {
        acc += faster_log_sum_exp_3(&(logps[ranges[i].offset]));
    }
    for(; i < n && ranges[i].width == 4; ++i) {
        acc += faster_log_sum_exp_4(&(logps[ranges[i].offset]));
    }
    for(; i < n && ranges[i].width == 5; ++i) {
        acc += faster_log_sum_exp_5(&(logps[ranges[i].offset]));
    }
    for(; i < n && ranges[i].width == 6; ++i) {
        acc += faster_log_sum_exp_6(&(logps[ranges[i].offset]));
    }
    for(; i < n && ranges[i].width == 7; ++i) {
        acc += faster_log_sum_exp_7(&(logps[ranges[i].offset]));
    }
    for(; i < n && ranges[i].width == 8; ++i) {
        acc += faster_log_sum_exp_8(&(logps[ranges[i].offset]));
    }
    for(; i < n && ranges[i].width == 9; ++i) {
        acc += faster_log_sum_exp_9(&(logps[ranges[i].offset]));
    }
    for(; i < n && ranges[i].width == 10; ++i) {
        acc += faster_log_sum_exp_10(&(logps[ranges[i].offset]));
    }
    return acc;
}


void sample_uniform(double *a, int n, double min, double max) {
    int i;
    double range = (max - min); 
    double div = RAND_MAX / range;

    for (i=0; i<n; ++i) {
        a[i] = min + (rand() / div);
    }
}


void sample_ranges(range_t *ranges, int n, int w, int m) {
    int i, width, offset;
    for (i=0; i<n; ++i) {
        // sample width from 1 to w
        width = rand() % (w + 1 - 1) + 1;
        // sample offset from 0 to m-width
        offset = rand() % (m-width + 1);
        ranges[i].offset = offset;
        ranges[i].width = width;
    }
}



int compare_ranges(const void *a, const void *b) {
    range_t *aa, *bb;
    aa = (range_t*)a;
    bb = (range_t*)b;
    int delta_w, delta_o;
    delta_w = (aa->width - bb->width);
    delta_o = (aa->offset - bb->offset);
    // return (delta_o != 0) ? delta_o : delta_w; // bad order
    return (delta_w != 0) ? delta_w : delta_o; // good order
}

void sort_ranges_inplace(range_t *ranges, int n) {
    qsort((void *)ranges, n, sizeof(range_t), compare_ranges);
}


void batch_log_inplace(double *a, int n) {
    int i;
    for (i=0; i<n; ++i) {
        a[i] = log(a[i]);
    }
}


int main(int argc, char **argv) {
    unsigned int seed;
    int n, m, w, i, trials, j, err;
    double *logps;

    range_t *ranges;
    double acc;

    int mode=-1;

    jit_reduction_func_t jf;
    jf.m = NULL;
    jf.size = 0;
    jf.f = NULL;

    if (argc <= 1) {
        printf("set mode=base\n");
        mode = MODE_BASE;
    } else {
        if (strcmp(argv[1], "base") == 0) {
            printf("set mode=base\n");
            mode = MODE_BASE;
        } else if (strcmp(argv[1], "fast") == 0) {
            printf("set mode=fast\n");
            mode = MODE_FAST;
        } else if (strcmp(argv[1], "onlysum") == 0) {
            printf("set mode=onlysum\n");
            mode = MODE_ONLY_SUM;
        } else if (strcmp(argv[1], "faster") == 0) {
            printf("set mode=faster\n");
            mode = MODE_FASTER;
        } else if (strcmp(argv[1], "fasterbb") == 0) {
            printf("set mode=fasterbb\n");
            mode = MODE_FASTERBB;
        } else if (strcmp(argv[1], "jit") == 0) {
            printf("set mode=jit\n");
            mode = MODE_JIT;
        } else {
            printf("unrecognised mode, expected one of 'base', 'fast', 'faster', 'fasterbb', 'jit', 'onlysum'\n");
            exit(1);
        }
    }

    printf("init\n");

    seed = 12345;
    srand(seed);

    m = 1000;
    logps = malloc(m * sizeof(double));
    sample_uniform(logps, m, 0.0, 1.0);
    batch_log_inplace(logps, m);

    n = 5000;
    w = 10;
    ranges = malloc(n * sizeof(range_t));
    sample_ranges(ranges, n, w, m);

    // for (i = 0; i < n; ++i) {
    //     printf("log_sum_exp,%d,%d\n", ranges[i].offset, ranges[i].width);
    // }
    // return 0;

    //  ranges              "faster"            branch
    //                      running time (s)    misses (%)
    //  ------              ----------------    -------
    //  unsorted            0.760               7.59
    //  sorted by offset    0.734               6.74
    //  sorted by width     0.534               0.02
    sort_ranges_inplace(ranges, n);

    trials = 10000;

    acc = 0.0;
    printf("ready\n");
    if (mode == MODE_BASE) {
        for (j = 0; j < trials; ++j) {
            for (i = 0; i < n; ++i) {
                acc += log_sum_exp(&(logps[ranges[i].offset]), ranges[i].width);
            }
        }
    } else if (mode == MODE_FAST) {
        for (j = 0; j < trials; ++j) {
            for (i = 0; i < n; ++i) {
                acc += fast_log_sum_exp(&(logps[ranges[i].offset]), ranges[i].width);
            }
        }
    } else if (mode == MODE_ONLY_SUM) {
        for (j = 0; j < trials; ++j) {
            for (i = 0; i < n; ++i) {
                acc += sum(&(logps[ranges[i].offset]), ranges[i].width);
            }
        }
    } else if (mode == MODE_FASTER) {
        for (j = 0; j < trials; ++j) {
            for (i = 0; i < n; ++i) {
                acc += faster_log_sum_exp(&(logps[ranges[i].offset]), ranges[i].width);
            }
        }
    } else if (mode == MODE_FASTERBB) {
        for (j = 0; j < trials; ++j) {
            logps[0] += acc; // impede optimisation
            acc += faster_log_sum_exp_bb(ranges, logps, n);
            logps[0] -= acc; // impede optimisation
       }
    } else if (mode == MODE_JIT) {
        printf("jit: input pattern has %d ranges with total size %zu bytes\n", n, n * sizeof(range_t));
        printf("jit: generating code\n");
        err = make_batch_log_sum_exp_jit_reduction_func(ranges, n, &jf);
        if (err != 0) {
            perror("err: make_batch_log_sum_exp_jit_reduction_func");
            return err;
        }
        printf("jit: generated %zu bytes of code\n", jf.size);
        printf("jit: switching mode RW -> RX\n");
        err = arm_jit_reduction_func(&jf);
        if (err != 0) {
            perror("err: arm_jit_reduction_func");
            err = release_jit_reduction_func(&jf);
            if (err != 0) {
                perror("err: release_jit_reduction_func");
            }
            return err;
        }
        printf("jit: ready\n");

        for (j = 0; j < trials; ++j) {
            acc += jf.f(logps, ranges, n);
        }

        err = release_jit_reduction_func(&jf);
        if (err != 0) {
            perror("err: release_jit_reduction_func");
        }
    }


    printf("done\n");

    printf("acc = %g\n", acc);
    return 0;
}
