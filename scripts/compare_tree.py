"""
generate gnu assembler code to compare an array
of 0 to 10 doubles.

generates code using scalar max ops

TODO: SIMD?
"""

def codegen_max(dst, src1, src2):
    # vmaxsd %xmm3,%xmm1,%xmm1
    print('vmaxsd %s,%s,%s' % (register(src2), register(src1), register(dst)))


def codegen_load(src_index, dst_register):
    # vmovsd 0x10(%rdi),%xmm3
    sizeof_double = 8
    print('vmovsd 0x%02x(%%rdi),%s' % (sizeof_double * src_index, register(dst_register)))


def codegen_ninf():
    print('movabs $0xfff0000000000000,%rcx')
    print('vmovq  %%rcx,%s' % (register(0), ))


def register(i):
    base = 3 # implementation-detail
    return '%%xmm%d' % (i + base, )


def codegen_max_tree(n):
    if n == 0:
        codegen_ninf()
        return

    generation = set([i for i in range(n)])

    n_generations = 0
    n_compares = 0

    while len(generation) > 1:
        n_generations += 1
        unmerged = generation
        next_generation = set()
        while len(unmerged) >= 2:
            a = min(unmerged)
            unmerged.remove(a)
            b = min(unmerged)
            unmerged.remove(b)
            codegen_max(a, a, b)
            next_generation.add(a)
            n_compares += 1
        next_generation |= unmerged
        generation = next_generation


def codegen_load_data(n):
    for i in range(n):
        src_index = i
        dst_register = i
        codegen_load(src_index, dst_register)


def main():
    for n in range(0, 10 + 1):
        print('.section CODE_MAX_OF_%d' % (n, ))
        codegen_load_data(n)
        print()
        codegen_max_tree(n)
        print()

if __name__ == '__main__':
    main()


