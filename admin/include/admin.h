#ifndef ADMIN_H
#define ADMIN_H
#include <defines.h>
#define ADMIN_SOCK_PATH "/tmp/aavarana_admin.sock"

#define CMD_KICK_USER 0x01
#define CMD_ROTATE_KEYS 0x02


typedef struct __attribute__((packed)) AdminPayload {
    u8 opcode;
    u32 target_ip;
} AdminPayload;


i32 admin_connect_socket();
void admin_dispatch_command(i32 fd, AdminPayload* payload);
void admin_await_response(i32 fd);

#endif // ADMIN_H