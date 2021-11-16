#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
#define MODE_FASTERB 5


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


double fasterb_log_sum_exp(double *a, int n) {
    // preconditions:
    // -inf <= a[i] <= 0.0 for all i = 0, ..., n-1
    double a_max, acc, acc_0, acc_1;
    int i, m;
    a_max = -INFINITY;
    for (i = 0; i < n; ++i) {
        a_max = fmax(a[i], a_max);
    }
    if (a_max <= -INFINITY || n <= 1) {
        return a_max;
    }
    m = n - (n % 2);
    acc_0 = 0.0;
    acc_1 = 0.0;
    for (i = 0; i < m; i += 2) {
        acc_0 += fast_exp(a[i] - a_max);
        acc_1 += fast_exp(a[i+1] - a_max);
    }
    acc = acc_0 + acc_1;
    if (m != n) {
        acc += fast_exp(a[n-1] - a_max);
    }
    return fast_log(acc) + a_max;
}




void sample_uniform(double *a, int n, double min, double max) {
    int i;
    double range = (max - min); 
    double div = RAND_MAX / range;

    for (i=0; i<n; ++i) {
        a[i] = min + (rand() / div);
    }
}


void sample_ranges(int *start, int *end, int n, int w, int m) {
    int i, width, offset;
    for (i=0; i<n; ++i) {
        // sample width from 1 to w
        width = rand() % (w + 1 - 1) + 1;
        // sample offset from 0 to m-width
        offset = rand() % (m-width + 1);
        start[i] = offset;
        end[i] = offset + width;
    }
}


void batch_log_inplace(double *a, int n) {
    int i;
    for (i=0; i<n; ++i) {
        a[i] = log(a[i]);
    }
}


int main(int argc, char **argv) {
    unsigned int seed;
    int n, m, w, i, trials, j;
    double *logps;
    int *start, *end;
    double acc;

    int mode=-1;
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
        } else if (strcmp(argv[1], "fasterb") == 0) {
            printf("set mode=fasterb\n");
            mode = MODE_FASTERB;
        } else {
            printf("unrecognised mode, expected one of 'base', 'fast', 'faster', 'fasterb', 'onlysum'\n");
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
    start = malloc(n * sizeof(int));
    end = malloc(n * sizeof(int));
    sample_ranges(start, end, n, w, m);

    trials = 10000;

    acc = 0.0;
    printf("ready\n");
    if (mode == MODE_BASE) {
        for (j = 0; j < trials; ++j) {
            for (i = 0; i < n; ++i) {
                acc += log_sum_exp(&(logps[start[i]]), end[i] - start[i]);
            }
        }
    } else if (mode == MODE_FAST) {
        for (j = 0; j < trials; ++j) {
            for (i = 0; i < n; ++i) {
                acc += fast_log_sum_exp(&(logps[start[i]]), end[i] - start[i]);
            }
        }
    } else if (mode == MODE_ONLY_SUM) {
        for (j = 0; j < trials; ++j) {
            for (i = 0; i < n; ++i) {
                acc += sum(&(logps[start[i]]), end[i] - start[i]);
            }
        }
    } else if (mode == MODE_FASTER) {
        for (j = 0; j < trials; ++j) {
            for (i = 0; i < n; ++i) {
                acc += faster_log_sum_exp(&(logps[start[i]]), end[i] - start[i]);
            }
        }
    } else if (mode == MODE_FASTERB) {
        for (j = 0; j < trials; ++j) {
            for (i = 0; i < n; ++i) {
                acc += fasterb_log_sum_exp(&(logps[start[i]]), end[i] - start[i]);
            }
        }
    }

    printf("done\n");

    printf("acc = %g\n", acc);
    return 0;
}
