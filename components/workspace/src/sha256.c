#include "workspace_sha256.h"
#include <stdint.h>

struct sha256_state
{
    uint32_t      words[8];
    uint64_t      bit_count;
    unsigned char block[64];
    size_t        block_size;
};

static uint32_t sha256_rotate(uint32_t value, unsigned int amount);
static void     sha256_transform(struct sha256_state *state, const unsigned char block[64]);
static void     sha256_update(struct sha256_state *state, const unsigned char *data, size_t length);
static void     sha256_finish(struct sha256_state *state, unsigned char digest[32]);

static const uint32_t SHA256_CONSTANTS[64] = {0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
                                              0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
                                              0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
                                              0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

void p101_workspace_sha256(const unsigned char *data, size_t length, char output[65])
{
    static const char   HEX[] = "0123456789abcdef";
    struct sha256_state state;
    unsigned char       digest[32];

    state.words[0]   = 0x6a09e667U;
    state.words[1]   = 0xbb67ae85U;
    state.words[2]   = 0x3c6ef372U;
    state.words[3]   = 0xa54ff53aU;
    state.words[4]   = 0x510e527fU;
    state.words[5]   = 0x9b05688cU;
    state.words[6]   = 0x1f83d9abU;
    state.words[7]   = 0x5be0cd19U;
    state.bit_count  = 0U;
    state.block_size = 0U;
    sha256_update(&state, data, length);
    sha256_finish(&state, digest);
    for(size_t index = 0U; index < sizeof(digest); index++)
    {
        output[index * 2U]      = HEX[digest[index] >> 4U];
        output[index * 2U + 1U] = HEX[digest[index] & 0x0fU];
    }
    output[64] = '\0';
}

static uint32_t sha256_rotate(uint32_t value, unsigned int amount)
{
    uint64_t widened;
    uint64_t rotated;

    widened = value;
    rotated = (widened >> amount) | (widened << (32U - amount));
    return (uint32_t)(rotated & UINT32_MAX);
}

static void sha256_transform(struct sha256_state *state, const unsigned char block[64])
{
    uint32_t schedule[64];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;

    for(size_t index = 0U; index < 16U; index++)
    {
        size_t offset;

        offset          = index * 4U;
        schedule[index] = (uint32_t)(((uint64_t)block[offset] << 24U) | ((uint64_t)block[offset + 1U] << 16U) | ((uint64_t)block[offset + 2U] << 8U) | (uint64_t)block[offset + 3U]);
    }
    for(size_t index = 16U; index < 64U; index++)
    {
        uint32_t first;
        uint32_t second;

        first           = sha256_rotate(schedule[index - 15U], 7U) ^ sha256_rotate(schedule[index - 15U], 18U) ^ (schedule[index - 15U] >> 3U);
        second          = sha256_rotate(schedule[index - 2U], 17U) ^ sha256_rotate(schedule[index - 2U], 19U) ^ (schedule[index - 2U] >> 10U);
        schedule[index] = (uint32_t)((uint64_t)schedule[index - 16U] + first + schedule[index - 7U] + second);
    }
    a = state->words[0];
    b = state->words[1];
    c = state->words[2];
    d = state->words[3];
    e = state->words[4];
    f = state->words[5];
    g = state->words[6];
    h = state->words[7];
    for(size_t index = 0U; index < 64U; index++)
    {
        uint32_t choose;
        uint32_t majority;
        uint32_t sigma_zero;
        uint32_t sigma_one;
        uint32_t temporary_one;
        uint32_t temporary_two;

        choose        = (e & f) ^ ((~e) & g);
        majority      = (a & b) ^ (a & c) ^ (b & c);
        sigma_zero    = sha256_rotate(a, 2U) ^ sha256_rotate(a, 13U) ^ sha256_rotate(a, 22U);
        sigma_one     = sha256_rotate(e, 6U) ^ sha256_rotate(e, 11U) ^ sha256_rotate(e, 25U);
        temporary_one = (uint32_t)((uint64_t)h + sigma_one + choose + SHA256_CONSTANTS[index] + schedule[index]);
        temporary_two = (uint32_t)((uint64_t)sigma_zero + majority);
        h             = g;
        g             = f;
        f             = e;
        e             = (uint32_t)((uint64_t)d + temporary_one);
        d             = c;
        c             = b;
        b             = a;
        a             = (uint32_t)((uint64_t)temporary_one + temporary_two);
    }
    state->words[0] = (uint32_t)((uint64_t)state->words[0] + a);
    state->words[1] = (uint32_t)((uint64_t)state->words[1] + b);
    state->words[2] = (uint32_t)((uint64_t)state->words[2] + c);
    state->words[3] = (uint32_t)((uint64_t)state->words[3] + d);
    state->words[4] = (uint32_t)((uint64_t)state->words[4] + e);
    state->words[5] = (uint32_t)((uint64_t)state->words[5] + f);
    state->words[6] = (uint32_t)((uint64_t)state->words[6] + g);
    state->words[7] = (uint32_t)((uint64_t)state->words[7] + h);
}

static void sha256_update(struct sha256_state *state, const unsigned char *data, size_t length)
{
    for(size_t index = 0U; index < length; index++)
    {
        state->block[state->block_size] = data[index];
        state->block_size++;
        if(state->block_size == sizeof(state->block))
        {
            sha256_transform(state, state->block);
            state->bit_count += 512U;
            state->block_size = 0U;
        }
    }
}

static void sha256_finish(struct sha256_state *state, unsigned char digest[32])
{
    uint64_t total_bits;

    total_bits                      = state->bit_count + (uint64_t)state->block_size * 8U;
    state->block[state->block_size] = 0x80U;
    state->block_size++;
    if(state->block_size > 56U)
    {
        while(state->block_size < sizeof(state->block))
        {
            state->block[state->block_size] = 0U;
            state->block_size++;
        }
        sha256_transform(state, state->block);
        state->block_size = 0U;
    }
    while(state->block_size < 56U)
    {
        state->block[state->block_size] = 0U;
        state->block_size++;
    }
    for(size_t index = 0U; index < 8U; index++)
    {
        state->block[63U - index] = (unsigned char)(total_bits >> (index * 8U));
    }
    sha256_transform(state, state->block);
    for(size_t index = 0U; index < 8U; index++)
    {
        digest[index * 4U]      = (unsigned char)(state->words[index] >> 24U);
        digest[index * 4U + 1U] = (unsigned char)(state->words[index] >> 16U);
        digest[index * 4U + 2U] = (unsigned char)(state->words[index] >> 8U);
        digest[index * 4U + 3U] = (unsigned char)state->words[index];
    }
}
