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


double baseline_fmax(double *a, int n) {
    int i;
    double acc_max = -INFINITY;
    for (i=0; i < n; ++i) {
        acc_max = fmax(a[i], acc_max);
    }
    return acc_max;
}


int make_jit_reduction_func(const unsigned char *code, size_t size, jit_reduction_func_t *out) {
    int status = 0;
    size_t alloc_size = ((size / 1024) + 1) * 1024;
	void* m = mmap(0, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); // RW
    if (m == 0) {
        return 1;
    }
	memcpy(m, code, size);
	status = mprotect(m,alloc_size, PROT_READ | PROT_EXEC); // RX
    if (status != 0) {
        return status;
    }
    out->f = m;
    out->m = m;
    out->size = alloc_size;
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

const unsigned char CODE_FMAX_HEADER[] = {
    0xc5, 0xf9, 0x57, 0xc0, // vxorpd %xmm0,%xmm0,%xmm0
    0x48, 0xb9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, // movabs $0xfff0000000000000,%rcx // aka -inf
    0xc4, 0xe1, 0xf9, 0x6e, 0xc9  // vmovq  %rcx,%xmm1
};

const unsigned char CODE_FMAX_FOOTER[] = {
    0xc5, 0xf9, 0x28, 0xc1, // vmovapd %xmm1,%xmm0
    0xc3 // retq
};


unsigned char* make_fmax_code(int n, size_t *size) {
    // Generate code for computing the max of an array of n doubles,
    // where the address of the array is in rdi
    // Values of n from 0 to 10 are supported.
    // return value:  pointer to generated code
    // the size of the generated code is written to size.
    //
    // TODO FIXME
    // generated code is simple, not efficient:
    //   max( ... max(max(-inf, a[0]), a[1]), ..., a[n-1])
    int total_size = 0, iota, i;
    unsigned char *code = NULL;
    total_size += sizeof(CODE_FMAX_HEADER);
    for (i = 0; i < 10; ++i) {
        if (n >= i+1) {
            total_size += CODESIZE_LOAD_A_XMM3[i] + sizeof(CODE_MAX_XMM3_XMM1_XMM1);
        }
    }
    total_size += sizeof(CODE_FMAX_FOOTER);

    code = (unsigned char*)malloc(total_size * sizeof(unsigned char));
    iota = 0;
    memcpy(code + iota, CODE_FMAX_HEADER, sizeof(CODE_FMAX_HEADER)); iota += sizeof(CODE_FMAX_HEADER);

    for (i = 0; i < 10; ++i) {
        if (n >= i+1) {
            memcpy(code + iota, CODE_LOAD_A_XMM3[i], CODESIZE_LOAD_A_XMM3[i]);
            iota += CODESIZE_LOAD_A_XMM3[i];
            memcpy(code + iota, CODE_MAX_XMM3_XMM1_XMM1, sizeof(CODE_MAX_XMM3_XMM1_XMM1));
            iota += sizeof(CODE_MAX_XMM3_XMM1_XMM1);
        }
    }
    memcpy(code + iota, CODE_FMAX_FOOTER, sizeof(CODE_FMAX_FOOTER)); iota += sizeof(CODE_FMAX_FOOTER);
    *size = iota;
    return code;
}



int main() {
    int err = 0, i;
    double data[10], expected, actual;
    unsigned char *code;
    size_t code_size;
    data[0] = -1.2;
    data[1] = 3.4;
    data[2] = 5.6;
    data[3] = 7.8;
    data[4] = 9.1;
    data[5] = 11.12;
    data[6] = 13.14;
    data[7] = 14.15;
    data[8] = 16.17;
    data[9] = 18.19;

    jit_reduction_func_t jf;

    for(i = 0; i < 11; ++i) {
        expected = baseline_fmax(data, i);

        code = make_fmax_code(i, &code_size);
        err = make_jit_reduction_func(code, code_size, &jf);
        free(code);
        if (err != 0) {
            perror("err: make_jit_reduction_func");
            return err;
        }
        actual = jf.f(data);
        err = munmap(jf.m, jf.size);
        if (err != 0) {
            perror("err: munmap");
            return err;
        }
        jf.f = NULL;
        jf.size = 0;

        printf("baseline result = %g\n", expected);
        printf("jit      result = %g\n", actual);
    }



	return 0;
}
