#ifndef AUTH_DAEMON_H
#define AUTH_DAEMON_H
#include <defines.h>


typedef struct AuthRequest {
    i8 username[32];
    i8 password[32];
} AuthRequest;

typedef struct AuthResponse {
    i32 status;
    u32 virtual_ip;
    u8 session_key[16];
} AuthResponse;

pid_t auth_spawn_worker(i32 pipe_down[2], i32 pipe_up[2]);
u8 auth_verify_credentials(const i8* username, const i8* password);
u32 auth_allocate_lease();
void auth_release_lease(u32 ip);

#endif // AUTH_DAEMON_H