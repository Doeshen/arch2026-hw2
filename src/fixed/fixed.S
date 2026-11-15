# op1, op2, op3 and q_mul: hand-written RV32I.
#
# uint32_t op1_asm(uint32_t a, uint32_t b, int *n)   a0, a1, a2 -> a0
# uint32_t op2_asm(uint32_t a, uint32_t b)           a0, a1 -> a0
# uint32_t op3_asm(uint32_t a, uint32_t b)           a0, a1 -> a0
# int32_t  q_mul_asm(int32_t a, int32_t b)           a0, a1 -> a0
#
# RV32I and Zicsr only: no mul, div, rem, no floating point, no compressed
# instructions. q_mul forms a 64-bit product in a register pair. Tests:
# tests/tfixed.c.

    .text
    .globl op1_asm
    .type op1_asm, @function
    .align 2

op1_asm:
    li t1, 0
    beqz a1, op1_done

op1_loop:
    and t0, a0, a1
    slli t0, t0, 1
    xor a0, a0, a1
    mv a1, t0
    addi t1, t1, 1
    bnez a1, op1_loop

op1_done:
    sw t1, 0(a2)
    ret

    .size op1_asm, .-op1_asm


    .globl op2_asm
    .type op2_asm, @function
    .align 2
op2_asm:
    sub a0, a0, a1
    ret
    .size   op2_asm, .-op2_asm

    .globl  op3_asm
    .type   op3_asm, @function
    .align  2
op3_asm:
    and t0, a0, a1
    xor t1, a0, a1
    srli t1, t1, 1
    add a0, t0, t1
    ret

    .size op3_asm, .-op3_asm



    .globl q_mul_asm
    .type q_mul_asm, @function
    .align 2

q_mul_asm:
    mv t0, a0
    srai t1, a0, 31
    mv t2, a1

    li t3, 0
    li t4, 0
    li t5, 32

qmul_loop:
    andi t6, t2, 1
    beqz t6, qmul_skip_add

    add t3, t3, t0
    sltu t6, t3, t0
    add t4, t4, t1
    add t4, t4, t6

qmul_skip_add:
    srli t6, t0, 31
    slli t1, t1, 1
    or t1, t1, t6
    slli t0, t0, 1

    srli t2, t2, 1
    addi t5, t5, -1
    bnez t5, qmul_loop

    bge a1, zero, qmul_round
    sub t4, t4, a0

qmul_round:
    li t6, 0x80000
    add t3, t3, t6
    sltu t6, t3, t6
    add t4, t4, t6

    srli t3, t3, 20
    slli t4, t4, 12
    or a0, t3, t4

    ret

    .size q_mul_asm, .-q_mul_asm
