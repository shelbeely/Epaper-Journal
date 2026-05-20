#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// test/mocks/mbedtls/pkcs5.h — stub for mbedTLS PKCS#5 PBKDF2 API
//
// The stub derives a key by XOR-folding the password and salt bytes into the
// output buffer. This is NOT secure but gives deterministic, different keys
// for different passwords/salts, which is sufficient for logic tests.
// ─────────────────────────────────────────────────────────────────────────────

#include "md.h"
#include <cstdint>
#include <cstring>

inline int mbedtls_pkcs5_pbkdf2_hmac(mbedtls_md_context_t* /*ctx*/,
                                      const unsigned char* password, size_t plen,
                                      const unsigned char* salt,     size_t slen,
                                      unsigned int /*iterations*/,
                                      uint32_t key_length,
                                      unsigned char* output) {
    std::memset(output, 0, key_length);
    for (size_t i = 0; i < plen; i++)
        output[i % key_length] ^= password[i];
    for (size_t i = 0; i < slen; i++)
        output[(plen + i) % key_length] ^= salt[i];
    return 0;
}
