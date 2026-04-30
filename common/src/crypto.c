#include <crypto.h>


void crypto_init(CipherContext* ctx, const u8* key, i32 key_length) {
        memcpy(ctx->key, key, key_length);
        ctx->key_length = key_length;
}


void crypto_process_stream(CipherContext* ctx, u8* payload, i32 length) {
    for(int i = 0; i < length; i++) {
        payload[i] = payload[i] ^ ctx->key[i % ctx->key_length];
    }
}