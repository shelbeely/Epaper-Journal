#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// test/mocks/mbedtls/gcm.h — stub for mbedTLS AES-GCM API
//
// The stub uses an identity cipher (output == input).
// The 16-byte tag encodes key[0] in tag[0] so that wrong-key detection works:
//   encrypt with key A → tag[0] = key[0]
//   decrypt with key B → tag[0] != key[0] → auth failure → returns -1
// ─────────────────────────────────────────────────────────────────────────────

#include <cstddef>
#include <cstdint>
#include <cstring>

#define MBEDTLS_CIPHER_ID_AES  1
#define MBEDTLS_GCM_ENCRYPT    1
#define MBEDTLS_GCM_DECRYPT    0

struct mbedtls_gcm_context {
    uint8_t  key[32];
    uint32_t key_bits;
};

inline void mbedtls_gcm_init(mbedtls_gcm_context* ctx) {
    std::memset(ctx, 0, sizeof(*ctx));
}
inline void mbedtls_gcm_free(mbedtls_gcm_context* /*ctx*/) {}

inline int mbedtls_gcm_setkey(mbedtls_gcm_context* ctx,
                               int /*cipher_id*/,
                               const uint8_t* key,
                               uint32_t key_bits) {
    std::memcpy(ctx->key, key, key_bits / 8);
    ctx->key_bits = key_bits;
    return 0;
}

// Stub encrypt: identity cipher; tag[0] = key[0] as an "auth" marker.
inline int mbedtls_gcm_crypt_and_tag(mbedtls_gcm_context* ctx,
                                      int /*mode*/,
                                      size_t length,
                                      const uint8_t* /*iv*/, size_t /*iv_len*/,
                                      const uint8_t* /*aad*/, size_t /*aad_len*/,
                                      const uint8_t* input, uint8_t* output,
                                      size_t tag_len, uint8_t* tag) {
    std::memcpy(output, input, length);
    std::memset(tag, 0, tag_len);
    if (tag_len > 0) tag[0] = ctx->key[0];
    return 0;
}

// Stub decrypt: verify tag[0] == key[0], then identity copy.
inline int mbedtls_gcm_auth_decrypt(mbedtls_gcm_context* ctx,
                                     size_t length,
                                     const uint8_t* /*iv*/, size_t /*iv_len*/,
                                     const uint8_t* /*aad*/, size_t /*aad_len*/,
                                     const uint8_t* tag, size_t /*tag_len*/,
                                     const uint8_t* input, uint8_t* output) {
    if (tag[0] != ctx->key[0]) return -1;  // auth failure (wrong key)
    std::memcpy(output, input, length);
    return 0;
}
