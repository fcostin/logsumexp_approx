// ref: https://eli.thegreenplace.net/2013/11/05/how-to-jit-an-introduction

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <math.h>


typedef double (*reduction_func_t)(double *);


typedef struct {
    reduction_func_t f;
    void *m;
    size_t size;
} jit_reduction_func_t;


double baseline_log_sum_exp(double *a, int n) {
    int i;
    double acc_max = -INFINITY;
    double acc = 0.0;
    for (i=0; i < n; ++i) {
        acc_max = fmax(a[i], acc_max);
    }
    // todo: if acc_max <= -inf, return that immediately
    for (i=0; i < n; ++i) {
        acc += exp(a[i] - acc_max);
    }
    return log(acc) + acc_max;
}


int allocate_jit_reduction_func(size_t size, jit_reduction_func_t *jf) {
    size_t alloc_size = ((size / 1024) + 1) * 1024;
	void* m = mmap(0, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); // RW
    if (m == 0) {
        jf->f = NULL;
        jf->m = NULL;
        jf->size = 0;
        return 1;
    }
    jf->f = NULL;
    jf->m = m;
    jf->size = alloc_size;
    return 0;
}

int arm_jit_reduction_func(jit_reduction_func_t *jf) {
    int status;
    if (jf->m == NULL) {
        return 1;
    }
	status = mprotect(jf->m, jf->size, PROT_READ | PROT_EXEC); // RX
    if (status != 0) {
        return status;
    }
    jf->f = (reduction_func_t)jf->m;
    return status;
}

int release_jit_reduction_func(jit_reduction_func_t *jf) {
    int status;
    jf->f = NULL;
    if (jf->m == NULL) {
        jf->size = 0;
        return 0;
    }
    status = munmap(jf->m, jf->size);
    if (status != 0) {
        return status;
    }
    jf->size = 0;
    return 0;
}


const unsigned char CODE_RETURN_NEG_INF[] = {
    0xc5, 0xf9, 0x57, 0xc0, // vxorpd %xmm0,%xmm0,%xmm0
    0x48, 0xb9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, // movabs $0xfff0000000000000,%rcx // aka -inf
    0xc4, 0xe1, 0xf9, 0x6e, 0xc9, // vmovq  %rcx,%xmm1
    0xc5, 0xf9, 0x28, 0xc1, // vmovapd %xmm1,%xmm0
    0xc3 // retq
};

const unsigned char CODE_RETURN_A2_FMAX[] = {
    0xc5, 0xf9, 0x57, 0xc0, // vxorpd %xmm0,%xmm0,%xmm0

    0x48, 0xb9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, // movabs $0xfff0000000000000,%rcx // aka -inf
    0xc4, 0xe1, 0xf9, 0x6e, 0xc9, // vmovq  %rcx,%xmm1

    0xc5, 0xfb, 0x10, 0x1f, // vmovsd (%rdi),%xmm3
    0xc5, 0xf3, 0x5f, 0xcb, // vmaxsd %xmm3,%xmm1,%xmm1

    0xc5, 0xfb, 0x10, 0x5f, 0x08, // vmovsd 0x8(%rdi),%xmm3
    0xc5, 0xf3, 0x5f, 0xcb, // vmaxsd %xmm3,%xmm1,%xmm1

    0xc5, 0xf9, 0x28, 0xc1, // vmovapd %xmm1,%xmm0
    0xc3 // retq
};


const unsigned char CODE_LOAD_A0_XMM3[] = {
   0xc5, 0xfb, 0x10, 0x1f //vmovsd (%rdi),%xmm3
};

const unsigned char CODE_LOAD_A1_XMM3[] = {
   0xc5, 0xfb, 0x10, 0x5f, 0x08 //vmovsd 0x8(%rdi),%xmm3
};

const unsigned char CODE_LOAD_A2_XMM3[] = {
   0xc5, 0xfb, 0x10, 0x5f, 0x10 //vmovsd 0x10(%rdi),%xmm3
};

const unsigned char CODE_LOAD_A3_XMM3[] = {
   0xc5, 0xfb, 0x10, 0x5f, 0x18
};

const unsigned char CODE_LOAD_A4_XMM3[] = {
   0xc5, 0xfb, 0x10, 0x5f, 0x20
};

const unsigned char CODE_LOAD_A5_XMM3[] = {
   0xc5, 0xfb, 0x10, 0x5f, 0x28
};

const unsigned char CODE_LOAD_A6_XMM3[] = {
   0xc5, 0xfb, 0x10, 0x5f, 0x30
};

const unsigned char CODE_LOAD_A7_XMM3[] = {
   0xc5, 0xfb, 0x10, 0x5f, 0x38
};

const unsigned char CODE_LOAD_A8_XMM3[] = {
   0xc5, 0xfb, 0x10, 0x5f, 0x40
};

const unsigned char CODE_LOAD_A9_XMM3[] = {
   0xc5, 0xfb, 0x10, 0x5f, 0x48
};


const unsigned char* CODE_LOAD_A_XMM3[] = {
    CODE_LOAD_A0_XMM3,
    CODE_LOAD_A1_XMM3,
    CODE_LOAD_A2_XMM3,
    CODE_LOAD_A3_XMM3,
    CODE_LOAD_A4_XMM3,
    CODE_LOAD_A5_XMM3,
    CODE_LOAD_A6_XMM3,
    CODE_LOAD_A7_XMM3,
    CODE_LOAD_A8_XMM3,
    CODE_LOAD_A9_XMM3
};

const size_t CODESIZE_LOAD_A_XMM3[] = {
    sizeof(CODE_LOAD_A0_XMM3),
    sizeof(CODE_LOAD_A1_XMM3),
    sizeof(CODE_LOAD_A2_XMM3),
    sizeof(CODE_LOAD_A3_XMM3),
    sizeof(CODE_LOAD_A4_XMM3),
    sizeof(CODE_LOAD_A5_XMM3),
    sizeof(CODE_LOAD_A6_XMM3),
    sizeof(CODE_LOAD_A7_XMM3),
    sizeof(CODE_LOAD_A8_XMM3),
    sizeof(CODE_LOAD_A9_XMM3),
};



const unsigned char CODE_MAX_XMM3_XMM1_XMM1[] = {
    0xc5, 0xf3, 0x5f, 0xcb // vmaxsd %xmm3,%xmm1,%xmm1
};

/*
 xmm0 -- accumulates overall result
 xmm1 -- per reduction, accumulates acc_max
 xmm2 -- per reduction, accumulates acc
*/

const unsigned char CODE_LOG_SUM_EXP_HEADER[] = {
    0xc5, 0xf9, 0x57, 0xc0 // vxorpd %xmm0,%xmm0,%xmm0
};

const unsigned char CODE_FMAX_HEADER[] = {
    0x48, 0xb9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, // movabs $0xfff0000000000000,%rcx // aka -inf
    0xc4, 0xe1, 0xf9, 0x6e, 0xc9  // vmovq  %rcx,%xmm1
};


const unsigned char CODE_ACC_FAST_EXP_HEADER[] = {
    0xc5, 0xe9, 0x57, 0xd2, // vxorpd %xmm2,%xmm2,%xmm2  #acc = 0.0.
    0x48, 0xb9, 0xfe, 0x82, 0x2b, 0x65, 0x47, 0x15, 0x37, 0x43, // movabs $0x43371547652b82fe,%rcx
    0xc4, 0xe1, 0xf9, 0x6e, 0xe1, // vmovq  %rcx,%xmm4  # xmm4 = constant approx_factor
    0x48, 0xb9, 0x00, 0x00, 0x80, 0x3f, 0x89, 0xf7, 0xcf, 0x43, // movabs $0x43cff7893f800000,%rcx
    0xc4, 0xe1, 0xf9, 0x6e, 0xe9, // vmovq  %rcx,%xmm5  # xmm5 = constant approx_term
    0x48, 0xb9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x86, 0xc0, // movabs $0xc086100000000000,%rcx
    0xc4, 0xe1, 0xf9, 0x6e, 0xf1 // vmovq  %rcx,%xmm6  # xmm6 = constant fast_exp_min_arg
};

// fast exp cycle assumes that
//   xmm3 contains a[i]
//   xmm1 contans acc_max


const unsigned char CODE_ACC_FAST_EXP_CYCLE[] = {
    0xc5, 0xe3, 0x5c, 0xd9, // vsubsd %xmm1,%xmm3,%xmm3  # xmm3 = xmm3 - xmm1  # BEWARE arg order.

    0xc5, 0xf9, 0x28, 0xfc, // vmovapd %xmm4,%xmm7
    0xc4, 0xe2, 0xe1, 0xa9, 0xfd, // xmm7 = fma(xmm7, xmm3, xmm5)  # BEWARE arg order.

    0xc5, 0xcb, 0xc2, 0xdb, 0x02, // vcmplesd %xmm3,%xmm6,%xmm3  # guard against too small arg

    0xc4, 0xe1, 0xfb, 0x2c, 0xcf, // vcvttsd2si %xmm7,%rcx
    0xc4, 0xe1, 0xf9, 0x6e, 0xf9, // vmovq  %rcx,%xmm7

    0xc5, 0xe1, 0x54, 0xdf, // vandpd %xmm7,%xmm3,%xmm3  # guard against too small arg
    0xc5, 0xeb, 0x58, 0xd3, // vaddsd %xmm3,%xmm2,%xmm2  # acc += fast_exp(a[i] - acc_max)

    // debug constants
    // 0xc5, 0xf9, 0x28, 0xc4 // vmovapd %xmm4,%xmm0  . LGTM
    // 0xc5, 0xf9, 0x28, 0xc5 // vmovapd %xmm5,%xmm0  . LGTM
    // 0xc5, 0xf9, 0x28, 0xc6 // vmovapd %xmm6,%xmm0  . LGTM
};


const unsigned char CODE_FAST_LOG[] = {
	// prepare constants for fast log
	0xc5, 0xc1, 0x57, 0xff, // vxorpd %xmm7,%xmm7,%xmm7 // xmm7 = 0.0  . used in a couple of spots
	0x48, 0xb9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, // movabs $0xfff0000000000000,%rcx
	0xc4, 0xe1, 0xf9, 0x6e, 0xe1, // vmovq  %rcx,%xmm4  # xmm4 = constant -inf
	0x48, 0xb9, 0xef, 0x39, 0xfa, 0xfe, 0x42, 0x2e, 0xa6, 0x3c, // movabs $0x3ca62e42fefa39ef,%rcx
	0xc4, 0xe1, 0xf9, 0x6e, 0xe9, // vmovq  %rcx,%xmm5  # xmm5 = constant inv_approx_factor
	0x48, 0xb9, 0x20, 0x24, 0x35, 0x1e, 0x65, 0x28, 0x86, 0xc0, // movabs $0xc08628651e352420,%rcx
	0xc4, 0xe1, 0xf9, 0x6e, 0xf1, // vmovq  %rcx,%xmm6  # xmm6 = constant inv_approx_term

	// compute fast log
	0xc4, 0xe1, 0xf9, 0x7e, 0xd0, // vmovq  %xmm2,%rax  # fast log approx
	// note: xmm7 copied into high 64 bits of xmm3, which are then ignored.
	0xc4, 0xe1, 0xc3, 0x2a, 0xd8, // vcvtsi2sd %rax,%xmm7,%xmm3  # fast log approx
	0xc4, 0xe2, 0xd1, 0xa9, 0xde, // vfmadd213sd %xmm6,%xmm5,%xmm3  # fast log approx
	0xc5, 0xc3, 0xc2, 0xd2, 0x01, // vcmpltsd %xmm2,%xmm7,%xmm2  # guard against nonpositive arg
	0xc4, 0xe3, 0x59, 0x4b, 0xd3, 0x20, // vblendvpd %xmm2,%xmm3,%xmm4,%xmm2  # guard against nonpositive arg

	// accumulate
	0xc5, 0xf3, 0x58, 0xd2, // vaddsd %xmm2,%xmm1,%xmm2  # acc = fast_log(acc) + acc_max
	0xc5, 0xfb, 0x58, 0xc2 // vaddsd %xmm2,%xmm0,%xmm0  # result += acc
};



const unsigned char CODE_LOG_SUM_EXP_FOOTER[] = {
    0xc5, 0xf9, 0x28, 0xc2, // vmovapd %xmm2,%xmm0  # result += acc
    0xc3 // retq
};


int make_log_sum_exp_jit_reduction_func(int n, jit_reduction_func_t *jf) {
    // Generate code for computing the log sum exp of an array of n doubles,
    // where the address of the array is in rdi
    // Values of n from 0 to 10 are supported.
    //
    // IMPLEMENTED
    //
    // - first pass to compute acc_max
    //
    // - the second pass to compute acc = sum(fast_exp(a[i] - acc_max))
    //
    // - the third pass to compute fast_log(acc) + acc_max
    //
    // TODO FIXME
    // - the first pass to compute acc_max daisy chains n max operations.
    //   it is simple but will have unnecessarily high latency.
    //
    // - there is no test for acc_max <= -inf and early return.
    //
    int total_size = 0, iota, i, status;
    unsigned char *code = NULL;
    total_size += sizeof(CODE_LOG_SUM_EXP_HEADER);
    total_size += sizeof(CODE_FMAX_HEADER);
    for (i = 0; i < 10; ++i) {
        if (n >= i+1) {
            total_size += CODESIZE_LOAD_A_XMM3[i] + sizeof(CODE_MAX_XMM3_XMM1_XMM1);
        }
    }
    total_size += sizeof(CODE_ACC_FAST_EXP_HEADER);
    for (i = 0; i < 10; ++i) {
        if (n >= i+1) {
            total_size += CODESIZE_LOAD_A_XMM3[i] + sizeof(CODE_ACC_FAST_EXP_CYCLE);
        }
    }
    total_size += sizeof(CODE_FAST_LOG);
    total_size += sizeof(CODE_LOG_SUM_EXP_FOOTER);

    status = allocate_jit_reduction_func(total_size * sizeof(unsigned char), jf);
    if (status != 0) {
        return status;
    }
    code = (unsigned char*)jf->m;


    iota = 0;

    memcpy(code + iota, CODE_LOG_SUM_EXP_HEADER, sizeof(CODE_LOG_SUM_EXP_HEADER)); iota += sizeof(CODE_LOG_SUM_EXP_HEADER);

    memcpy(code + iota, CODE_FMAX_HEADER, sizeof(CODE_FMAX_HEADER)); iota += sizeof(CODE_FMAX_HEADER);

    for (i = 0; i < 10; ++i) {
        if (n >= i+1) {
            memcpy(code + iota, CODE_LOAD_A_XMM3[i], CODESIZE_LOAD_A_XMM3[i]);
            iota += CODESIZE_LOAD_A_XMM3[i];
            memcpy(code + iota, CODE_MAX_XMM3_XMM1_XMM1, sizeof(CODE_MAX_XMM3_XMM1_XMM1));
            iota += sizeof(CODE_MAX_XMM3_XMM1_XMM1);
        }
    }
    memcpy(code + iota, CODE_ACC_FAST_EXP_HEADER, sizeof(CODE_ACC_FAST_EXP_HEADER)); iota += sizeof(CODE_ACC_FAST_EXP_HEADER);
    for (i = 0; i < 10; ++i) {
        if (n >= i+1) {
            memcpy(code + iota, CODE_LOAD_A_XMM3[i], CODESIZE_LOAD_A_XMM3[i]);
            iota += CODESIZE_LOAD_A_XMM3[i];
            memcpy(code + iota, CODE_ACC_FAST_EXP_CYCLE, sizeof(CODE_ACC_FAST_EXP_CYCLE));
            iota += sizeof(CODE_ACC_FAST_EXP_CYCLE);
        }
    }
    memcpy(code + iota, CODE_FAST_LOG, sizeof(CODE_FAST_LOG)); iota += sizeof(CODE_FAST_LOG);
    memcpy(code + iota, CODE_LOG_SUM_EXP_FOOTER, sizeof(CODE_LOG_SUM_EXP_FOOTER)); iota += sizeof(CODE_LOG_SUM_EXP_FOOTER);

    return 0;
}



int main() {
    int err = 0, i;
    double data[10], expected, actual;

    data[0] = log(1.0/55.0);
    data[1] = log(2.0/55.0);
    data[2] = log(3.0/55.0);
    data[3] = log(4.0/55.0);
    data[4] = log(5.0/55.0);
    data[5] = log(6.0/55.0);
    data[6] = log(7.0/55.0);
    data[7] = log(8.0/55.0);
    data[8] = log(9.0/55.0);
    data[9] = log(10.0/55.0);

    jit_reduction_func_t jf;
    jf.f = NULL;
    jf.m = NULL;
    jf.size = 0;

    for(i = 0; i < 11; ++i) {
        expected = baseline_log_sum_exp(data, i);

        err = make_log_sum_exp_jit_reduction_func(i, &jf);
        if (err != 0) {
            perror("err: make_log_sum_exp_jit_reduction_func");
            return err;
        }
        err = arm_jit_reduction_func(&jf);
        if (err != 0) {
            perror("err: arm_jit_reduction_func");
            err = release_jit_reduction_func(&jf);
            if (err != 0) {
                perror("err: release_jit_reduction_func");
            }
            return err;
        }
        actual = jf.f(data);

        err = release_jit_reduction_func(&jf);
        if (err != 0) {
            perror("err: release_jit_reduction_func");
            return err;
        }
        jf.f = NULL;
        jf.size = 0;

        printf("baseline result = %g\n", expected);
        printf("jit      result = %g\n", actual);
    }

	return 0;
}
