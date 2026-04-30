#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "server_net.h"
#include "state.h"
#include "tun.h"
#include "defines.h"

// ---------------------------------------------------------
// 1. Define the Global State Variables
// ---------------------------------------------------------
RoutingTable global_rt;
KeyStore global_ks;
i32 global_tun_fd = -1;
i32 global_udp_fd = -1;

int main(void) {
    printf("[Server] Initializing Aavarana Server...\n");

    // 2. Initialize the State Engine (Mutex Locks & RAM arrays)
    state_init(&global_rt, &global_ks);

    // 3. Allocate the Server's TUN interface
    i8 tun_name[16] = "tun0"; 
    global_tun_fd = tun_alloc(tun_name);
    
    if (global_tun_fd < 0) {
        printf("[Server] Fatal: Could not allocate TUN interface. Are you root?\n");
        exit(1);
    }

    // 4. Assign the Server its Gateway IP (10.0.0.1)
    // inet_addr automatically returns Network Byte Order, which configure_tun_ip expects!
    printf("[Server] Configuring Gateway Interface...\n");
    configure_tun_ip(tun_name, inet_addr("10.0.0.1"));

    // 5. Initialize Network Sockets (TCP, UDP, Unix, Epoll)
    NetworkContext ctx;
    
    // NOTE: Make sure you fixed the spelling to net_init_sockets in server_net.c!
    net_init_sockets(&ctx); 

    // 6. THE MISSING GLUE: Link the local UDP fd to the global variable
    // This allows handle_tun_read() to send packets out to the clients.
    global_udp_fd = ctx.udp_fd;

    // 7. Start the Epoll Event Loop (Blocks forever)
    net_epoll_loop(&ctx);

    return 0;
}