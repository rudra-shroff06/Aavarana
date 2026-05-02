#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>
#include <client.h>
#include <crypto.h>


void client_bootstrap(i32 argc, i8* argv[], ClientConfig* config) {
    if (argc != 4) {
        printf("Usage: %s <Server_IP> <Username> <Password>\n", argv[0]);
        exit(1);
    }


    strncpy((i8*)config->server_ip, argv[1], sizeof(config->server_ip) - 1);
    strncpy((i8*)config->username, argv[2], sizeof(config->username) - 1);
    strncpy((i8*)config->password, argv[3], sizeof(config->password) - 1);


    config->server_ip[sizeof(config->server_ip) - 1] = '\0';
    config->username[sizeof(config->username) - 1] = '\0';
    config->password[sizeof(config->password) - 1] = '\0';


    init_tunconfig(&config->tconf);
    strcpy((i8*)config->tconf.dev, "tun0");
    
    config->tconf.fd = tun_alloc(config->tconf.dev);
    if (config->tconf.fd < 0) {
        printf("[Client] Fatal: Failed to allocate TUN interface.\n");
        exit(1);
    }
    printf("[Client] Bootstrap complete. TUN allocated as %s\n", config->tconf.dev);
}


i32 client_perform_handshake(ClientConfig* config, AuthResponse* resp_out) {
    i32 tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_fd < 0) return 0;

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);
    inet_pton(AF_INET, config->server_ip, &serv_addr.sin_addr);

    printf("[Client] Connecting to Control Plane (TCP:8080)...\n");
    if (connect(tcp_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[Client] Connection failed");
        close(tcp_fd);
        return 0;
    }


    AuthRequest req;
    memset(&req, 0, sizeof(AuthRequest));
    strncpy(req.username, config->username, 31);
    strncpy(req.password, config->password, 31);


    write(tcp_fd, &req, sizeof(AuthRequest));


    memset(resp_out, 0, sizeof(AuthResponse));
    i32 bytes_read = read(tcp_fd, resp_out, sizeof(AuthResponse));
    

    close(tcp_fd);

    if (bytes_read == sizeof(AuthResponse) && resp_out->status == 1) {
        printf("[Client] Authenticated! Virtual IP: %u\n", ntohl(resp_out->virtual_ip));
        return 1;
    }

    printf("[Client] Authentication Rejected by Server.\n");
    return 0;
}


void configure_client_routing(const char* server_ip, u32 virtual_ip) {
    char cmd[512];
    printf("[Client] Configuring Full Tunnel Routing...\n");


    snprintf(cmd, sizeof(cmd), 
             "sudo ip route add %s $(ip route show default | awk '/default/ {print \"via \"$3\" dev \"$5}')", 
             server_ip);
    system(cmd);


    system("sudo ip route add 0.0.0.0/1 via 10.0.0.1 dev tun0");
    system("sudo ip route add 128.0.0.0/1 via 10.0.0.1 dev tun0");


    system("sudo resolvectl dns tun0 8.8.8.8 8.8.4.4"); 
    
    printf("[Client] All internet traffic is now routed through the VPN!\n");
}


void client_run_tunnel(ClientConfig* config) {
    AuthResponse current_config;
    u8 debug_key[16] = "SUPERSECRETKEY12";

    while(1) {
        if (!client_perform_handshake(config, &current_config)) {
            printf("[Client] Handshake failed. Retrying...\n");
            sleep(5);
            continue;
        }

        configure_tun_ip(config->tconf.dev, current_config.virtual_ip);
        configure_client_routing(config->server_ip, current_config.virtual_ip);

        i32 udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in server_udp_addr;
        memset(&server_udp_addr, 0, sizeof(server_udp_addr));
        server_udp_addr.sin_family = AF_INET;
        server_udp_addr.sin_port = htons(1194);
        inet_pton(AF_INET, config->server_ip, &server_udp_addr.sin_addr);

        struct pollfd fds[2];
        fds[0].fd = config->tconf.fd;
        fds[0].events = POLLIN;
        fds[1].fd = udp_fd;
        fds[1].events = POLLIN;

        printf("[Client] Data Plane Online. Syncing with %s...\n", debug_key);

        while(1) {
            i32 ret = poll(fds, 2, -1); 
            if (ret == 0) { break; }

            u8 buffer[2048];
            CipherContext ctx;

            if (fds[0].revents & POLLIN) {
                i32 len = tun_read(config->tconf.fd, (i8*)buffer, sizeof(buffer));
                if (len > 0) {
                    
                    crypto_init(&ctx, debug_key, 16); 
                    crypto_process_stream(&ctx, buffer, len);
                    sendto(udp_fd, buffer, len, 0, (struct sockaddr*)&server_udp_addr, sizeof(server_udp_addr));
                }
            }

            if (fds[1].revents & POLLIN) {
                i32 len = recvfrom(udp_fd, buffer, sizeof(buffer), 0, NULL, NULL);
                if (len > 0) {
                   
                    crypto_init(&ctx, debug_key, 16); 
                    crypto_process_stream(&ctx, buffer, len);
                    tun_write(config->tconf.fd, (i8*)buffer, len);
                }
            }
        }
        close(udp_fd);
    }
}