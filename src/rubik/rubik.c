#include "rubik.h"

#include <stdint.h>

enum { CUBIES = 7, FACES = 3, MOVES = 9, MAX_DEPTH = 14 };

typedef struct {
    uint8_t p[CUBIES];
    uint8_t o[CUBIES];
} cube_t;

/* IMPORTANT: indices 0, 1, 2 correspond to R, B, D by HW2 naming. */
static const uint8_t source[FACES][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
};
static const uint8_t twist[FACES][CUBIES] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};
static const char *const move_name[MOVES] = {"R",  "R2", "R'", "B", "B2",
                                             "B'", "D",  "D2", "D'"};

static uint8_t move_from[MOVES][CUBIES];
static uint8_t move_twist[MOVES][CUBIES];
static uint8_t perm_dist[5040];
static uint8_t ori_dist[729];

/* Three-corner pattern database. */
static uint8_t corner_dist[9261];
static uint16_t corner_queue[9261];
static uint8_t inverse_move[MOVES][CUBIES];
static int tables_ready;


static void init_moves(void)
{
    for (int face = 0; face < FACES; ++face) {
        uint8_t p[CUBIES], o[CUBIES];
        for (int i = 0; i < CUBIES; ++i) {
            p[i] = (uint8_t) i;
            o[i] = 0;
        }
        for (int t = 0; t < 3; ++t) {
            uint8_t np[CUBIES], no[CUBIES];
            for (int i = 0; i < CUBIES; ++i) {
                int from = source[face][i];
                np[i] = p[from];
                int value = o[from] + twist[face][i];
                if (value >= 3)
                    value -= 3;
                no[i] = (uint8_t) value;
            }
            for (int i = 0; i < CUBIES; ++i) {
                p[i] = np[i];
                o[i] = no[i];
                move_from[face * 3 + t][i] = np[i];
                move_twist[face * 3 + t][i] = no[i];
            }
        }
    }
}


static int divide_small(int number, int divisor, int *remainder)
{
    unsigned quotient = 0;
    unsigned rem = 0;

    for (int bit = 12; bit >= 0; --bit) {
        rem = (rem << 1) | (((unsigned) number >> bit) & 1u);

        if (rem >= (unsigned) divisor) {
            rem -= (unsigned) divisor;
            quotient |= 1u << bit;
        }
    }

    *remainder = (int) rem;
    return (int) quotient;
}

static int is_multiple_of_three(int value)
{
    while (value >= 3)
        value -= 3;

    return value == 0;
}


/* Pack three selected corners into 15 bits.
 * Each corner uses 3 position bits and 2 orientation bits.
 */
static unsigned corner_key(uint16_t state)
{
    unsigned p0 = state & 7u;
    unsigned p1 = (state >> 5) & 7u;
    unsigned p2 = (state >> 10) & 7u;

    unsigned o0 = (state >> 3) & 3u;
    unsigned o1 = (state >> 8) & 3u;
    unsigned o2 = (state >> 13) & 3u;

    unsigned position = (p0 * 7u + p1) * 7u + p2;
    unsigned orientation = (o0 * 3u + o1) * 3u + o2;

    return position * 27u + orientation;
}

static void init_corner_tables(void)
{
    for (int i = 0; i < 9261; ++i)
        corner_dist[i] = 255;

    /* inverse_move[m][old_position] = new_position */
    for (int m = 0; m < MOVES; ++m) {
        for (int destination = 0; destination < CUBIES; ++destination) {
            int from = move_from[m][destination];
            inverse_move[m][from] = (uint8_t) destination;
        }
    }

    /* Corners 0, 1, 2 start at positions 0, 1, 2,
     * each with orientation zero.
     */
    uint16_t initial = (uint16_t) ((1u << 5) | (2u << 10));

    unsigned head = 0;
    unsigned tail = 0;

    corner_dist[corner_key(initial)] = 0;
    corner_queue[tail++] = initial;

    while (head < tail) {
        uint16_t state = corner_queue[head++];
        uint8_t distance = corner_dist[corner_key(state)];

        for (int m = 0; m < MOVES; ++m) {
            uint16_t next = 0;

            for (int k = 0; k < 3; ++k) {
                unsigned shift = (unsigned) k * 5u;

                unsigned position = (state >> shift) & 7u;
                unsigned orientation = (state >> (shift + 3u)) & 3u;

                unsigned new_position = inverse_move[m][position];
                unsigned new_orientation =
                    orientation + move_twist[m][new_position];

                if (new_orientation >= 3u)
                    new_orientation -= 3u;

                unsigned field = new_position | (new_orientation << 3u);

                next |= (uint16_t) (field << shift);
            }

            unsigned key = corner_key(next);

            if (corner_dist[key] == 255) {
                corner_dist[key] = (uint8_t) (distance + 1u);
                corner_queue[tail++] = next;
            }
        }
    }
}

static int corner_heuristic(const cube_t *cube)
{
    uint16_t state = 0;

    for (int position = 0; position < CUBIES; ++position) {
        unsigned cubie = cube->p[position];

        if (cubie < 3u) {
            unsigned shift = cubie * 5u;
            unsigned field =
                (unsigned) position | ((unsigned) cube->o[position] << 3u);

            state |= (uint16_t) (field << shift);
        }
    }

    return corner_dist[corner_key(state)];
}

static int perm_index(const uint8_t *p)
{
    int index = 0;
    for (int i = 0; i < CUBIES; ++i) {
        int smaller = 0;
        for (int j = i + 1; j < CUBIES; ++j)
            smaller += p[j] < p[i];
        index = index * (CUBIES - i) + smaller;
    }
    return index;
}

static void decode_perm(int index, uint8_t *p)
{
    static const int factorial[CUBIES] = {720, 120, 24, 6, 2, 1, 1};
    uint8_t available[CUBIES] = {0, 1, 2, 3, 4, 5, 6};
    for (int i = 0; i < CUBIES; ++i) {
        int at = 0;
        while (index >= factorial[i]) {
            index -= factorial[i];
            ++at;
        }
        p[i] = available[at];
        for (int j = at; j < CUBIES - i - 1; ++j)
            available[j] = available[j + 1];
    }
}

static int ori_index(const uint8_t *o)
{
    int index = 0;
    for (int i = 0; i < CUBIES - 1; ++i)
        index = index * 3 + o[i];
    return index;
}

static void decode_ori(int index, uint8_t *o)
{
    int sum = 0;

    for (int i = CUBIES - 2; i >= 0; --i) {
        int remainder;
        index = divide_small(index, 3, &remainder);
        o[i] = (uint8_t) remainder;
        sum += remainder;
    }

    while (sum >= 3)
        sum -= 3;

    o[CUBIES - 1] = (uint8_t) (sum == 0 ? 0 : 3 - sum);
}

static void init_tables(void)
{
    if (tables_ready)
        return;

    init_moves();

    for (int i = 0; i < 5040; ++i)
        perm_dist[i] = 255;

    for (int i = 0; i < 729; ++i)
        ori_dist[i] = 255;

    perm_dist[0] = 0;
    ori_dist[0] = 0;

    int count = 1;

    for (int depth = 0; count < 5040 && depth < 15; ++depth) {
        for (int index = 0; index < 5040; ++index) {
            if (perm_dist[index] != depth)
                continue;

            uint8_t p[CUBIES], next[CUBIES];
            decode_perm(index, p);

            for (int m = 0; m < MOVES; ++m) {
                for (int i = 0; i < CUBIES; ++i)
                    next[i] = p[move_from[m][i]];

                int ni = perm_index(next);

                if (perm_dist[ni] == 255) {
                    perm_dist[ni] = (uint8_t) (depth + 1);
                    ++count;
                }
            }
        }
    }

    count = 1;

    for (int depth = 0; count < 729 && depth < 15; ++depth) {
        for (int index = 0; index < 729; ++index) {
            if (ori_dist[index] != depth)
                continue;

            uint8_t o[CUBIES], next[CUBIES];
            decode_ori(index, o);

            for (int m = 0; m < MOVES; ++m) {
                for (int i = 0; i < CUBIES; ++i) {
                    int value = o[move_from[m][i]] + move_twist[m][i];

                    if (value >= 3)
                        value -= 3;

                    next[i] = (uint8_t) value;
                }

                int ni = ori_index(next);

                if (ori_dist[ni] == 255) {
                    ori_dist[ni] = (uint8_t) (depth + 1);
                    ++count;
                }
            }
        }
    }

    init_corner_tables();
    tables_ready = 1;
}

static int parse(const char *state, cube_t *cube)
{
    if (!state)
        return 0;
    uint32_t used = 0;
    int sum = 0;
    for (int i = 0; i < CUBIES; ++i) {
        char ch = state[i];
        if (ch < '1' || ch > '7')
            return 0;
        uint32_t bit = 1u << (ch - '1');
        if (used & bit)
            return 0;
        used |= bit;
        cube->p[i] = (uint8_t) (ch - '1');
    }
    for (int i = 0; i < CUBIES; ++i) {
        char ch = state[CUBIES + i];
        if (ch < '1' || ch > '3')
            return 0;
        cube->o[i] = (uint8_t) (ch - '1');
        sum += cube->o[i];
    }
    return state[14] == '\0' && is_multiple_of_three(sum);
}

__attribute__((optimize("O2"))) static int search(const cube_t *cube,
                                                  int remaining,
                                                  int last_face,
                                                  uint8_t *path,
                                                  int depth)
{
    if (corner_heuristic(cube) > remaining)
        return 0;

    int pd = perm_dist[perm_index(cube->p)];
    if (pd > remaining)
        return 0;

    int od = ori_dist[ori_index(cube->o)];
    if (od > remaining)
        return 0;

    if (pd == 0 && od == 0)
        return 1;

    if (remaining == 0)
        return 0;

    for (int move = 0; move < MOVES; ++move) {
        int face = move < 3 ? 0 : (move < 6 ? 1 : 2);
        if (face == last_face)
            continue;
        cube_t next;
        for (int i = 0; i < CUBIES; ++i) {
            int from = move_from[move][i];
            next.p[i] = cube->p[from];
            int o = cube->o[from] + move_twist[move][i];
            if (o >= 3)
                o -= 3;
            next.o[i] = (uint8_t) o;
        }
        path[depth] = (uint8_t) move;
        if (search(&next, remaining - 1, face, path, depth + 1))
            return 1;
    }
    return 0;
}

int rubik_solve(const char *state, char *out, size_t cap)
{
    cube_t cube;
    if (!parse(state, &cube))
        return -1;

    init_tables();
    uint8_t path[MAX_DEPTH];
    int steps = 0;
    for (; steps <= MAX_DEPTH; ++steps) {
        if (search(&cube, steps, -1, path, 0))
            break;
    }
    if (steps > MAX_DEPTH)
        return -1;

    size_t len = 0;
    for (int i = 0; i < steps; ++i) {
        if (i != 0)
            ++len;
        len += move_name[path[i]][1] ? 2u : 1u;
    }
    if (len + 1 > cap) {
        if (cap > 0 && out)
            out[0] = '\0';
        return -2;
    }
    size_t at = 0;
    for (int i = 0; i < steps; ++i) {
        if (i != 0)
            out[at++] = ' ';
        const char *name = move_name[path[i]];
        out[at++] = name[0];
        if (name[1])
            out[at++] = name[1];
    }
    out[at] = '\0';
    return steps;
}
