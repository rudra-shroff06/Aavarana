#ifndef CLIENT_H
#define CLIENT_H
#include <defines.h>
#include <tun.h>
#include <auth_daemon.h>

// Struct: ClientConfig
// Purpose: Holds Server IP, User Credentials, and allocated TunConfig
typedef struct ClientConfig {
    i8 server_ip[16];
    i8 username[32];
    i8 password[32];
    TunConfig tconf;
} ClientConfig;

// Function Prototypes directly from the Rubric
void client_bootstrap(i32 argc, i8* argv[], ClientConfig* config);
i32 client_perform_handshake(ClientConfig* config, AuthResponse* resp_out);
void client_run_tunnel(ClientConfig* config);

#endif // CLIENT_H