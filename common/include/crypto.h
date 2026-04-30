#ifndef CRYPTO_H
#define CRYPTO_H
#include <defines.h>
#define MAX_KEY_LENGTH 32

typedef struct CipherContext {
    u8 key[MAX_KEY_LENGTH];
    i32 key_length;
} CipherContext;

void crypto_init(CipherContext* ctx, const u8* key, i32 key_length);
void crypto_process_stream(CipherContext* ctx, u8* payload, i32 length);

#endif // CRYPTO_H