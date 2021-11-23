
# constants
#                 zero	               0	hex	0x0000000000000000
#                 -inf	            -inf	hex	0xfff0000000000000
#        approx_factor	     6.49732e+15	hex	0x43371547652b82fe
#          approx_term	     4.60692e+18	hex	0x43cff7893f800000
#     fast_exp_min_arg	            -706	hex	0xc086100000000000
#    inv_approx_factor	      1.5391e-16	hex	0x3ca62e42fefa39ef
#      inv_approx_term	        -709.049	hex	0xc08628651e352420





# inititialisation

# global state

vxorpd %xmm0,%xmm0,%xmm0  # result. xmm0 <- 0.0


# log_sum_exp reduction -- init local state

movq   $0xfff0000000000000, %rcx  # move immediate 64-bit floating point -INF bits into rcx
vmovq  %rcx,%xmm1 # acc_max. xmm1 <- -INF

vxorpd %xmm2,%xmm2,%xmm2  # acc. xmm2 <- 0.0


# log_sum_exp reduction pass 1 -- accumulate acc_max

# acc_max = max(x[0], acc_max)

vmovsd (%rdi),%xmm3             # xmm3 <- *(rdi + 0 * sizeof(double))
vmaxsd %xmm3,%xmm1,%xmm1

# acc_max = max(x[1], acc_max)

vmovsd 0x8(%rdi),%xmm3          # xmm3 <- *(rdi + 1 * sizeof(double))
vmaxsd %xmm3,%xmm1,%xmm1

# etc...




# log_sum_exp reduction pass 2 -- accumulate acc

# log_sum_exp reduction pass 3 -- compute log(acc) + acc_max

vmovapd %xmm1,%xmm0  # TODO FIXME placeholder implemenation, use acc_max as the result 


# wrapping up

retq
