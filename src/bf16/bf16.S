# BFloat16 routines: hand-written RV32I.
#
# uint16_t bf16_from_f32_asm(uint32_t f32bits)   a0 -> a0
# uint16_t bf16_mul_asm(uint16_t a, uint16_t b)  a0, a1 -> a0
# uint16_t bf16_sqrt_asm(uint16_t a)             a0 -> a0
#
# RV32I and Zicsr only: no mul, div, rem, no floating point, no compressed
# instructions. Tests: tests/tbf16.c.

    .text

    .globl bf16_from_f32_asm
    .type bf16_from_f32_asm, @function
    .align 2

bf16_from_f32_asm:
    srli t0, a0, 23
    andi t0, t0, 255

    li t1, 255
    beq t0, t1, bf16_f32_done

    srli t0, a0, 16
    andi t0, t0, 1

    li t1, 0x7FFF
    add a0, a0, t1
    add a0, a0, t0

bf16_f32_done:
    srli a0, a0, 16
    ret

    .size bf16_from_f32_asm, .-bf16_from_f32_asm

    .globl bf16_mul_asm
    .type bf16_mul_asm, @function
    .align 2

bf16_mul_asm:
    # Sign = sign(a) XOR sign(b)
    xor a2, a0, a1
    li t3, 0x8000
    and a2, a2, t3

    # Extract exponents
    srli a3, a0, 7
    andi a3, a3, 255
    srli a4, a1, 7
    andi a4, a4, 255

    # Extract fractions
    andi a5, a0, 127
    andi a6, a1, 127

    # Special cases: examine a first
    li t3, 255
    beq a3, t3, mul_special_a
    beq a4, t3, mul_special_b

    # Check zero operands
    li t3, 0x7FFF
    and t2, a0, t3
    beqz t2, mul_zero
    and t2, a1, t3
    beqz t2, mul_zero

    # Normalize significand a
    mv a7, a5
    bnez a3, mul_a_normal

    li a3, 1

mul_a_denorm:
    li t3, 128
    bgeu a7, t3, mul_a_ready
    slli a7, a7, 1
    addi a3, a3, -1
    j mul_a_denorm

mul_a_normal:
    ori a7, a7, 128

mul_a_ready:
    # Normalize significand b
    mv t0, a6
    bnez a4, mul_b_normal

    li a4, 1

mul_b_denorm:
    li t3, 128
    bgeu t0, t3, mul_b_ready
    slli t0, t0, 1
    addi a4, a4, -1
    j mul_b_denorm

mul_b_normal:
    ori t0, t0, 128

mul_b_ready:
    # Multiply two 8-bit significands
    # t1 = product
    # a7 = shifted multiplicand
    # t0 = remaining multiplier
    li t1, 0
    li t2, 8

mul_loop:
    andi t3, t0, 1
    beqz t3, mul_skip_add
    add t1, t1, a7

mul_skip_add:
    slli a7, a7, 1
    srli t0, t0, 1
    addi t2, t2, -1
    bnez t2, mul_loop

    # Exponent = ea + eb - 127
    add t4, a3, a4
    addi t4, t4, -127

    # Normalize product and truncate
    li t3, 32768
    bltu t1, t3, mul_product_small

    srli t5, t1, 8
    addi t4, t4, 1
    j mul_check_exp

mul_product_small:
    srli t5, t1, 7

mul_check_exp:
    # Overflow: exponent >= 255
    li t3, 255
    bge t4, t3, mul_inf

    # Underflow: exponent < -6
    li t3, -6
    blt t4, t3, mul_zero

    # Subnormal: exponent <= 0
    blt zero, t4, mul_normal_result

    li t3, 1
    sub t3, t3, t4
    andi t5, t5, 127
    srl t5, t5, t3
    or a0, a2, t5
    ret

mul_normal_result:
    slli t4, t4, 7
    andi t5, t5, 127
    or a0, a2, t4
    or a0, a0, t5
    ret

    # a has exponent 0xFF
mul_special_a:
    bnez a5, mul_return_a

    li t3, 0x7FFF
    and t2, a1, t3
    beqz t2, mul_nan
    j mul_inf

    # b has exponent 0xFF
mul_special_b:
    bnez a6, mul_return_b

    li t3, 0x7FFF
    and t2, a0, t3
    beqz t2, mul_nan
    j mul_inf

mul_return_a:
    ret

mul_return_b:
    mv a0, a1
    ret

mul_nan:
    li a0, 0x7FC0
    ret

mul_inf:
    li t3, 0x7F80
    or a0, a2, t3
    ret

mul_zero:
    mv a0, a2
    ret

    .size bf16_mul_asm, .-bf16_mul_asm


    .globl bf16_sqrt_asm
    .type bf16_sqrt_asm, @function
    .align 2

bf16_sqrt_asm:
    # Extract exponent and fraction.
    srli t0, a0, 7
    andi t0, t0, 255
    andi t1, a0, 127

    # Handle NaN and infinity.
    li t2, 255
    beq t0, t2, sqrt_special

    # Both signed zeros return +0.
    li t2, 0x7FFF
    and t2, a0, t2
    beqz t2, sqrt_zero

    # Negative nonzero values return NaN.
    li t2, 0x8000
    and t2, a0, t2
    bnez t2, sqrt_nan

    # Positive denormals return +0.
    beqz t0, sqrt_zero

    # Unbiased exponent.
    addi a2, t0, -127

    # Significand with hidden leading 1.
    ori a1, t1, 128

    # Make the exponent even.
    andi t2, a2, 1
    beqz t2, sqrt_even

    slli a1, a1, 1
    addi a2, a2, -1

sqrt_even:
    # limit = (mantissa + 1) * 128
    addi a3, a1, 1
    slli a3, a3, 7

    # Binary search, low valid, high invalid.
    li a4, 90
    li a5, 257

sqrt_search:
    add a6, a4, a5
    srli a6, a6, 1

    # Calculate mid * mid without MUL.
    mv t3, a6
    mv t4, a6
    li t5, 0

sqrt_square:
    andi t6, t3, 1
    beqz t6, sqrt_no_add

    add t5, t5, t4

sqrt_no_add:
    slli t4, t4, 1
    srli t3, t3, 1
    bnez t3, sqrt_square

    # Is mid * mid < limit?
    bltu t5, a3, sqrt_accept

    mv a5, a6
    j sqrt_next

sqrt_accept:
    mv a4, a6

sqrt_next:
    addi t2, a4, 1
    blt t2, a5, sqrt_search

    # Root is now in a4.
    li t2, 256
    bltu a4, t2, sqrt_pack

    srli a4, a4, 1
    addi a2, a2, 2

sqrt_pack:
    # Result exponent = exponent / 2 + 127.
    srai a2, a2, 1
    addi a2, a2, 127

    li t2, 255
    bge a2, t2, sqrt_inf
    bge zero, a2, sqrt_zero

    slli a2, a2, 7
    andi a4, a4, 127

    or a0, a2, a4
    ret

sqrt_special:
    # Return a NaN unchanged.
    bnez t1, sqrt_return

    # -Inf gives quiet NaN; +Inf stays +Inf.
    li t2, 0x8000
    and t2, a0, t2
    bnez t2, sqrt_nan
    ret

sqrt_zero:
    li a0, 0
    ret

sqrt_nan:
    li a0, 0x7FC0
    ret

sqrt_inf:
    li a0, 0x7F80
    ret

sqrt_return:
    ret

    .size bf16_sqrt_asm, .-bf16_sqrt_asm
