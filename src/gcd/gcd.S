
    .text
    .globl gcd_u32_asm
    .type gcd_u32_asm, @function
    .align 2

gcd_u32_asm:
    beqz a0, return_b
    beqz a1, done

    li t0, 0

common_twos:
    or t1, a0, a1
    andi t1, t1, 1
    bnez t1, make_a_odd

    srli a0, a0, 1
    srli a1, a1, 1
    addi t0, t0, 1
    j common_twos

make_a_odd:
    andi t1, a0, 1
    bnez t1, make_b_odd
    srli a0, a0, 1
    j make_a_odd

make_b_odd:
    andi t1, a1, 1
    bnez t1, compare
    srli a1, a1, 1
    j make_b_odd

compare:
    beq a0, a1, finish
    bltu a0, a1, reduce_b

reduce_a:
    sub a0, a0, a1
    srli a0, a0, 1

strip_a:
    andi t1, a0, 1
    bnez t1, compare
    srli a0, a0, 1
    j strip_a

reduce_b:
    sub a1, a1, a0
    srli a1, a1, 1

strip_b:
    andi t1, a1, 1
    bnez t1, compare
    srli a1, a1, 1
    j strip_b

finish:
    sll a0, a0, t0

done:
    ret

return_b:
    mv a0, a1
    ret

    .size gcd_u32_asm, .-gcd_u32_asm
