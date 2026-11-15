#include "fixed.h"

/* Dummies: every function returns 0. Replace with the real routines. */

uint32_t op1(uint32_t a, uint32_t b, int *n)
{
    int count = 0;

    while (b != 0) {
        uint32_t carry = (a & b) << 1;
        a = a ^ b;
        b = carry;
        ++count;
    }

    *n = count;
    return a;
}

uint32_t op2(uint32_t a, uint32_t b)
{
    return a - b;
}

uint32_t op3(uint32_t a, uint32_t b)
{
    return (a & b) + ((a ^ b) >> 1);
}


int32_t q_mul(int32_t a, int32_t b)
{
    uint32_t lo = 0;
    uint32_t hi = 0;
    uint32_t ml = (uint32_t) a;
    uint32_t mh = a < 0 ? UINT32_MAX : 0;
    uint32_t bits = (uint32_t) b;

    for (unsigned i = 0; i < 32; ++i) {
        if ((bits & 1u) != 0) {
            uint32_t old = lo;
            lo += ml;
            hi += mh + (lo < old);
        }

        mh = (mh << 1) | (ml >> 31);
        ml <<= 1;
        bits >>= 1;
    }

    if (b < 0) {
        hi -= (uint32_t) a;
    }

    hi += lo > UINT32_MAX - (1u << 19);
    lo += (1u << 19);

    return (int32_t) ((lo >> 20) | (hi << 12));
}
