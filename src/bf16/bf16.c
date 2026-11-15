#include "bf16.h"

/* Dummies: every function returns 0. Replace with the real routines. */
uint16_t bf16_from_f32(uint32_t f32bits)
{
    uint32_t exp = (f32bits >> 23) & 0xFFu;

    if (exp == 0xFFu) {
        return (uint16_t) (f32bits >> 16);
    }

    uint32_t lsb = (f32bits >> 16) & 1u;
    uint32_t rounded = f32bits + 0x7FFFu + lsb;

    return (uint16_t) (rounded >> 16);
}

uint16_t bf16_mul(uint16_t a, uint16_t b)
{
    uint16_t sign = (a ^ b) & 0x8000u;

    int ea = (a >> 7) & 0xFF;
    int eb = (b >> 7) & 0xFF;

    uint32_t fa = a & 0x7Fu;
    uint32_t fb = b & 0x7Fu;

    // Special case: examine a first.
    if (ea == 0xFF) {
        if (fa != 0)
            return a;

        if ((b & 0x7FFFu) == 0)
            return 0x7FC0u;

        return sign | 0x7F80u;
    }

    // Special case: examine b second.
    if (eb == 0xFF) {
        if (fb != 0)
            return b;

        if ((a & 0x7FFFu) == 0)
            return 0x7FC0u;

        return sign | 0x7F80u;
    }

    // Zero times a finite number.
    if ((a & 0x7FFFu) == 0 || (b & 0x7FFFu) == 0) {
        return sign;
    }

    // Build the 8-bit significands.
    uint32_t ma = fa;
    uint32_t mb = fb;

    if (ea == 0) {
        ea = 1;
        while (ma < 128u) {
            ma <<= 1;
            --ea;
        }
    } else {
        ma |= 0x80u;
    }

    if (eb == 0) {
        eb = 1;
        while (mb < 128u) {
            mb <<= 1;
            --eb;
        }
    } else {
        mb |= 0x80u;
    }

    // Significand multiplication.
    uint32_t product = ma * mb;
    int exponent = ea + eb - 127;

    // Normalize and truncate.
    uint32_t mantissa;

    if (product >= 32768u) {
        mantissa = product >> 8;
        ++exponent;
    } else {
        mantissa = product >> 7;
    }

    // Overflow to infinity.
    if (exponent >= 255)
        return sign | 0x7F80u;

    // Underflow to signed zero.
    if (exponent < -6)
        return sign;

    // Subnormal result.
    if (exponent <= 0) {
        mantissa = (mantissa & 0x7Fu) >> (1 - exponent);
        return sign | (uint16_t) mantissa;
    }
    // Normal result.
    return sign | (uint16_t) (exponent << 7) | (uint16_t) (mantissa & 0x7Fu);
}


uint16_t bf16_sqrt(uint16_t a)
{
    uint32_t exp = (a >> 7) & 0xFFu;
    uint32_t frac = a & 0x7Fu;

    // NaN and infinity.
    if (exp == 0xFFu) {
        if (frac != 0)
            return a;

        if ((a & 0x8000u) != 0)
            return 0x7FC0u;

        return a;
    }

    // Both +0 and -0 produce +0.
    if ((a & 0x7FFFu) == 0)
        return 0;

    // Negative nonzero values are invalid.
    if ((a & 0x8000u) != 0)
        return 0x7FC0u;

    // Positive denormals produce +0.
    if (exp == 0)
        return 0;

    int exponent = (int) exp - 127;
    uint32_t mantissa = 128u | frac;

    // Make exponent even.
    if ((exponent & 1) != 0) {
        mantissa <<= 1;
        --exponent;
    }

    // floor(mid * mid / 128) <= mantissa
    // is equivalent to:
    // mid * mid < (mantissa + 1) * 128.
    uint32_t limit = (mantissa + 1u) << 7;

    uint32_t low = 90;
    uint32_t high = 257;

    while (low + 1 < high) {
        uint32_t mid = (low + high) >> 1;

        if (mid * mid < limit)
            low = mid;
        else
            high = mid;
    }

    uint32_t root = low;

    // Renormalize if necessary.
    if (root >= 256u) {
        root >>= 1;
        exponent += 2;
    }

    int result_exp = exponent / 2 + 127;

    if (result_exp >= 255)
        return 0x7F80u;

    if (result_exp <= 0)
        return 0;

    return (uint16_t) ((result_exp << 7) | (root & 0x7Fu));
}
