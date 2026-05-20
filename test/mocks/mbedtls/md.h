#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// test/mocks/mbedtls/md.h — stub for mbedTLS message-digest API
// Used by OtaManager.cpp for SHA-256 verification.
// ─────────────────────────────────────────────────────────────────────────────

#include <cstddef>
#include <cstdint>

typedef enum {
    MBEDTLS_MD_NONE   = 0,
    MBEDTLS_MD_SHA256 = 6,
} mbedtls_md_type_t;

struct mbedtls_md_info_t  { int type; };
struct mbedtls_md_context_t { const mbedtls_md_info_t* info; };

inline void mbedtls_md_init(mbedtls_md_context_t* ctx) {
    ctx->info = nullptr;
}
inline const mbedtls_md_info_t* mbedtls_md_info_from_type(mbedtls_md_type_t /*t*/) {
    static mbedtls_md_info_t s_info{};
    return &s_info;
}
inline int mbedtls_md_setup(mbedtls_md_context_t* ctx,
                            const mbedtls_md_info_t* info, int /*hmac*/) {
    ctx->info = info;
    return 0;
}
inline int mbedtls_md_starts(mbedtls_md_context_t* /*ctx*/)                    { return 0; }
inline int mbedtls_md_update(mbedtls_md_context_t* /*ctx*/,
                             const uint8_t* /*input*/, size_t /*ilen*/)        { return 0; }
inline int mbedtls_md_finish(mbedtls_md_context_t* /*ctx*/,
                             uint8_t* /*output*/)                              { return 0; }
inline void mbedtls_md_free(mbedtls_md_context_t* ctx) { ctx->info = nullptr; }
