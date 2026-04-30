#ifndef SERVER_NET_H
#define SERVER_NET_H
#include <defines.h>

typedef struct NetworkContext {
    i32 udp_fd;
    i32 tcp_fd;
    i32 unix_fd;
    i32 epoll_fd;
} NetworkContext;

typedef struct EventCallBack {
    void (*on_read)(i32 fd);
    void (*on_write)(i32 fd);
    void (*on_error)(i32 fd);
} EventCallBack;

typedef struct __attribute__((packed)) {
    u8 opcode;       // 0x01 for Kick, 0x02 for Rotate Keys
    u32 target_ip;   // The Virtual IP to kick (Network Byte Order)
} AdminPayload;

void net_init_sockets(NetworkContext* ctx);
void net_epoll_loop(NetworkContext* ctx);


void handle_tcp_auth(i32 fd);
void handle_udp_tunnel(i32 fd);
void handle_admin_cmd(i32 fd);
void handle_tun_read(i32 fd);

#endif // SERVER_NET_H