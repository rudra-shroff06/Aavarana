#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>
#include <client.h>
#include <crypto.h>

// ----------------------------------------------------------------------------
// 1. client_bootstrap: Parses terminal args and allocates the local TUN
// ----------------------------------------------------------------------------
void client_bootstrap(i32 argc, i8* argv[], ClientConfig* config) {
    if (argc != 4) {
        printf("Usage: %s <Server_IP> <Username> <Password>\n", argv[0]);
        exit(1);
    }

    // Safely copy terminal arguments into the config struct
    strncpy((i8*)config->server_ip, argv[1], sizeof(config->server_ip) - 1);
    strncpy((i8*)config->username, argv[2], sizeof(config->username) - 1);
    strncpy((i8*)config->password, argv[3], sizeof(config->password) - 1);

    // Ensure null termination
    config->server_ip[sizeof(config->server_ip) - 1] = '\0';
    config->username[sizeof(config->username) - 1] = '\0';
    config->password[sizeof(config->password) - 1] = '\0';

    // Initialize and allocate the TUN interface using your tun.c logic
    init_tunconfig(&config->tconf);
    strcpy((i8*)config->tconf.dev, "tun0"); // Default name
    
    config->tconf.fd = tun_alloc(config->tconf.dev);
    if (config->tconf.fd < 0) {
        printf("[Client] Fatal: Failed to allocate TUN interface.\n");
        exit(1);
    }
    printf("[Client] Bootstrap complete. TUN allocated as %s\n", config->tconf.dev);
}

// ----------------------------------------------------------------------------
// 2. client_perform_handshake: TCP Auth, gets Virtual IP and Key
// ----------------------------------------------------------------------------
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

    // Construct the AuthRequest using your struct
    AuthRequest req;
    memset(&req, 0, sizeof(AuthRequest));
    strncpy(req.username, config->username, 31);
    strncpy(req.password, config->password, 31);

    // Send credentials
    write(tcp_fd, &req, sizeof(AuthRequest));

    // Wait for Server's AuthResponse
    memset(resp_out, 0, sizeof(AuthResponse));
    i32 bytes_read = read(tcp_fd, resp_out, sizeof(AuthResponse));
    
    // IMMEDIATELY CLOSE TCP TO PREVENT MELTDOWN
    close(tcp_fd);

    if (bytes_read == sizeof(AuthResponse) && resp_out->status == 1) {
        printf("[Client] Authenticated! Virtual IP: %u\n", ntohl(resp_out->virtual_ip));
        return 1;
    }

    printf("[Client] Authentication Rejected by Server.\n");
    return 0;
}

// ----------------------------------------------------------------------------
// 3. client_run_tunnel: The blocking UDP Data Plane with Self-Healing
// ----------------------------------------------------------------------------
void client_run_tunnel(ClientConfig* config) {
    AuthResponse current_config;

    // OUTER LOOP: Self-Healing & Re-Authentication Layer
    while (1) {
        // Step 1: Execute the Handshake
        if (!client_perform_handshake(config, &current_config)) {
            printf("[Client] Handshake failed. Retrying in 5 seconds...\n");
            sleep(5);
            continue;
        }

        // Step 2: Configure the OS routing and MTU (From your tun.c)
        configure_tun_ip(config->tconf.dev, current_config.virtual_ip);

        // Step 3: Initialize Crypto Context
        CipherContext ctx;
        crypto_init(&ctx, current_config.session_key, 16);

        // Step 4: Open the UDP Socket (The Data Tunnel)
        i32 udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in server_udp_addr;
        memset(&server_udp_addr, 0, sizeof(server_udp_addr));
        server_udp_addr.sin_family = AF_INET;
        server_udp_addr.sin_port = htons(1194);
        inet_pton(AF_INET, config->server_ip, &server_udp_addr.sin_addr);

        // Step 5: Setup poll() to watch TUN and UDP
        struct pollfd fds[2];
        fds[0].fd = config->tconf.fd;  // TUN (Traffic from OS -> VPN)
        fds[0].events = POLLIN;
        fds[1].fd = udp_fd;            // UDP (Traffic from VPN -> OS)
        fds[1].events = POLLIN;

        printf("[Client] UDP Data Plane Online. Routing packets...\n");

        // INNER LOOP: High-speed packet routing
        while (1) {
            // POLL WITH 15-SECOND TIMEOUT (Key Rotation Trigger)
            i32 ret = poll(fds, 2, 15000); 

            if (ret == 0) {
                printf("[Client] UDP Timeout! Triggering Re-Auth for Key Rotation...\n");
                close(udp_fd);
                break; // Break inner loop, jump to outer loop to fetch new key
            }

            u8 buffer[2048];

            // Event A: OS sent a packet into TUN (Outbound to Internet)
            if (fds[0].revents & POLLIN) {
                i32 len = tun_read(config->tconf.fd, (i8*)buffer, sizeof(buffer));
                if (len > 0) {
                    crypto_process_stream(&ctx, buffer, len);
                    sendto(udp_fd, buffer, len, 0, (struct sockaddr*)&server_udp_addr, sizeof(server_udp_addr));
                }
            }

            // Event B: Server sent a packet via UDP (Inbound from Internet)
            if (fds[1].revents & POLLIN) {
                i32 len = recvfrom(udp_fd, buffer, sizeof(buffer), 0, NULL, NULL);
                if (len > 0) {
                    crypto_process_stream(&ctx, buffer, len);
                    tun_write(config->tconf.fd, (i8*)buffer, len);
                }
            }
        }
    }
}