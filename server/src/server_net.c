#include <stdio.h>
#include <unistd.h>
#include <server_net.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/ip.h>
#include <auth_daemon.h>
#include <state.h>
#include <crypto.h>
#define MAX_EVENTS 64
#define TCP_LISTEN_BUFFER_SIZE 10
#define UNIX_SOCK_PATH "/tmp/aavarana_admin.sock"


extern RoutingTable global_rt;
extern KeyStore global_ks;
extern i32 global_tun_fd;
extern i32 global_udp_fd;

static EventCallBack tcp_cb = {.on_read = handle_tcp_auth, .on_write = NULL, .on_error = NULL};
static EventCallBack udp_cb = {.on_read = handle_udp_tunnel, .on_write = NULL, .on_error = NULL};
static EventCallBack unix_cb = {.on_read = handle_admin_cmd, .on_write = NULL, .on_error = NULL};



void net_init_sockets(NetworkContext* ctx) {
    ctx->epoll_fd = epoll_create1(0);
    if(ctx->epoll_fd == -1) {
        perror("[ERROR]");
        exit(1);
    }

    ctx->udp_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in udp_addr = {.sin_family = AF_INET, .sin_port = htons(1194), .sin_addr = INADDR_ANY};
    bind(ctx->udp_fd, (struct sockaddr*)&udp_addr, sizeof(udp_addr));

    ctx->tcp_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in tcp_addr = {.sin_family = AF_INET, .sin_port = htons(8080), .sin_addr = INADDR_ANY};
    bind(ctx->tcp_fd, (struct sockaddr*)&tcp_addr, sizeof(tcp_addr));
    listen(ctx->tcp_fd, TCP_LISTEN_BUFFER_SIZE);

    ctx->unix_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un unix_addr = {.sun_family = AF_UNIX, .sun_path = UNIX_SOCK_PATH};
    unlink(UNIX_SOCK_PATH);
    bind(ctx->unix_fd, (struct sockaddr*)&unix_addr, sizeof(unix_addr));
    listen(ctx->unix_fd, 5);

    struct epoll_event ev;
    ev.events = EPOLLIN;
    
    ev.data.fd = ctx->udp_fd;
    epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, ctx->udp_fd, &ev);

    ev.data.fd = ctx->tcp_fd;
    epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, ctx->tcp_fd, &ev);

    ev.data.fd = ctx->unix_fd;
    epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, ctx->unix_fd, &ev);

    ev.data.fd = global_tun_fd;
    epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, global_tun_fd, &ev);
}


void net_epoll_loop(NetworkContext* ctx) {
    struct epoll_event events[MAX_EVENTS];

    printf("[Net] Transport Mesh Online. Epoll Listening...\n");

    while(1) {
        i32 nfds = epoll_wait(ctx->epoll_fd, events, MAX_EVENTS, -1);
        for(int i = 0; i < nfds; i++) {
            i32 current_fd = events[i].data.fd;

            if(events[i].events & EPOLLIN) {
                if(current_fd == ctx->tcp_fd) {
                    tcp_cb.on_read(current_fd);
                }
                else if(current_fd == ctx->udp_fd) {
                    udp_cb.on_read(current_fd);
                }
                else if(current_fd == ctx->unix_fd) {
                    unix_cb.on_read(current_fd);
                }
                else if(current_fd == global_tun_fd) {
                    handle_tun_read(current_fd);
                }
            }
        }
    }

}

void handle_tcp_auth(i32 fd) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    i32 client_fd = accept(fd, (struct sockaddr*)&client_addr, &addr_len);
    if(client_fd == -1) return;

    AuthRequest req;
    int bytes_read = read(client_fd, &req, sizeof(AuthRequest));
    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }


    i32 pipe_down[2], pipe_up[2];
    pid_t pid = auth_spawn_worker(pipe_down, pipe_up);

    if(pid == 0) {
        AuthRequest child_req;
        read(pipe_down[0], &child_req, sizeof(AuthRequest));

        child_req.username[sizeof(child_req.username) - 1] = '\0';
        child_req.username[sizeof(child_req.password) - 1] = '\0';

        AuthResponse child_resp;
        memset(&child_resp, 0, sizeof(AuthResponse));

        if(auth_verify_credentials(child_req.username, child_req.password)) {
            child_resp.status = 1;
            child_resp.virtual_ip = auth_allocate_lease();
        }
        else {
            child_resp.status = 0;
        }
        write(pipe_up[1], &child_resp, sizeof(AuthResponse));

        exit(0);
    }
    else if(pid > 0) {
        write(pipe_down[1], &req, sizeof(AuthRequest));

        AuthResponse resp;
        read(pipe_up[0], &resp, sizeof(AuthResponse));

        waitpid(pid, NULL, 0);

        if(resp.status == 1) {
            u8 initial_key[16];

            state_add_client(&global_rt, &global_ks, resp.virtual_ip, &client_addr, initial_key);
            memcpy(resp.session_key, initial_key, 16);
            printf("[TCP] Client authenticate. Assigned IP: %u\n", ntohl(resp.virtual_ip));
        }
        else {
            printf("[TCP] Authentication failed for user: %s\n", req.username);
        }

        write(client_fd, &resp, sizeof(AuthResponse));
        close(client_fd);
    }
}


void handle_udp_tunnel(i32 fd) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    u8 buffer[2048];

    i32 len = recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &addr_len);
    if(len <= 0) return;

    pthread_mutex_lock(&global_ks.lock);
    CipherContext ctx;
    crypto_init(&ctx, global_ks.current_session_key, global_ks.key_len);
    pthread_mutex_unlock(&global_ks.lock);

    crypto_process_stream(&ctx, buffer, len);

    write(global_tun_fd, buffer, len);

}


void handle_admin_cmd(i32 fd) {
    i32 admin_fd = accept(fd, NULL, NULL);
    if(admin_fd < 0) return;

    AdminPayload payload;
    i32 bytes_read = read(admin_fd, &payload, sizeof(AdminPayload));

    if (bytes_read == sizeof(AdminPayload)) {
        
        if (payload.opcode == 0x02) { 
            // --- COMMAND: ROTATE KEYS ---
            printf("[Admin] Key rotation command received.\n");
            u8 new_key[16];
            for (int i = 0; i < 16; i++) new_key[i] = rand() % 256;
            
            state_trigger_key_rotation(&global_rt, &global_ks, new_key, 16);
            printf("[Admin] Keys rotated successfully.\n");
        }
        else if (payload.opcode == 0x01) { 
            // --- COMMAND: KICK USER ---
            printf("[Admin] Kick User command received for IP: %u\n", ntohl(payload.target_ip));

            // 1. Wipe them from RAM so they can't route packets anymore
            state_remove_client(&global_rt, payload.target_ip);

            // 2. THIS IS WHERE IT GOES: Release the IP back to leases.txt!
            auth_release_lease(payload.target_ip);

            printf("[Admin] User successfully purged from memory and lease released.\n");
        }
    }
    
    close(admin_fd);
}


void handle_tun_read(i32 tun_fd) {
    u8 buffer[2048];

    i32 len = read(tun_fd, buffer, sizeof(buffer));
    if(len <= 20) return;

    u32 dest_virtual_ip;
    memcpy(&dest_virtual_ip, &buffer[16], 4);

    struct sockaddr_in client_udp_addr;
    u8 local_session_key[16];
    i32 key_len = 0;
    i32 found = 0;

    pthread_mutex_lock(&global_rt.lock);
    pthread_mutex_lock(&global_ks.lock);
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(global_rt.entries[i].is_active && global_rt.entries[i].virtual_ip == dest_virtual_ip) {
            client_udp_addr = global_rt.entries[i].real_addr;
            
            memcpy(local_session_key, global_ks.current_session_key, global_ks.key_len);
            key_len = global_ks.key_len;
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&global_ks.lock);
    pthread_mutex_unlock(&global_rt.lock);
    if(!found) return;

    CipherContext ctx;
    crypto_init(&ctx, local_session_key, key_len);
    crypto_process_stream(&ctx, buffer, len);
}