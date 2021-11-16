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

double fast_exp(double x) {
    // FIXME: this gives bad results where x < -706.0 (roughly)
    // FIXME: this gives bad results where x = -inf
    union {
        long int i;
        double d;
    } b;
    b.i = (long int)(fma(APPROX_A, x, + (APPROX_B - APPROX_C)));
    return b.d;
}


double fast_log(double x) {
    // naively invert fast_exp
    // y = (a * x) + b
    // x = (y - b) / a
    union {
        long int i;
        double d;
    } b;
    b.d = x;
    return APPROX_A_INV * (((double)b.i) - (APPROX_B - APPROX_C));
}


int main(int argc, char **argv) {
    int i, n;
    double log_p, z, actual, expected, rel_error, actual_log;
    n = 350;
    log_p = log(0.1);
    for (i = 0; i < n; ++i) {
        z = (double)(i) * log_p;
        actual = fast_exp(z);
        expected = exp(z);
        rel_error = (expected - actual) / expected;
        actual_log = fast_log(actual);
        printf("i=%03d\tz=%g\tfastexp(z)=%g\texp(z)=%g\trel_error=%g\tfast_log(actual)=%g\n", i, z, actual, expected, rel_error, actual_log);
    }
    return 0;
}
