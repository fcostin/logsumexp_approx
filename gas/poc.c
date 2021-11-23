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


double baseline_fmax(double *a) {
	return fmax(-INFINITY, fmax(a[0], a[1]));
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




int main() {
    int err = 0;
    double data[2];
    data[0] = -4.3;
    data[1] = 88.123;

    jit_reduction_func_t jf;

    err = make_jit_reduction_func(CODE_RETURN_A2_FMAX, sizeof(CODE_RETURN_A2_FMAX), &jf);
    if (err != 0) {
        perror("err: make_jit_reduction_func");
        return err;
    }

	printf("baseline result = %g\n", baseline_fmax(data));
	printf("jit      result = %g\n", jf.f(data));

	return 0;
}
