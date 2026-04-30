#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h> // Make sure this is included at the top for open()
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



void configure_os_routing() {
    printf("[Server] Taking control of Linux Kernel Routing...\n");

    // 1. NATIVE C: Enable IP Forwarding by writing directly to the Linux /proc filesystem
    int fd = open("/proc/sys/net/ipv4/ip_forward", O_WRONLY);
    if (fd >= 0) {
        write(fd, "1\n", 2);
        close(fd);
        printf("[Server] [+] Native IP Forwarding enabled.\n");
    } else {
        printf("[Server] [-] Failed to enable IP forwarding. Are you root?\n");
    }

    // 2. SYSTEM SHELL: Inject the NAT rule into iptables
    // NOTE: Change 'eth0' to whatever your server's public interface is (check using 'ip a')
    int ret = system("iptables -t nat -A POSTROUTING -s 10.0.0.0/24 -o eth0 -j MASQUERADE");
    if (ret == 0) {
        printf("[Server] [+] NAT Masquerading injected into iptables.\n");
    } else {
        printf("[Server] [-] Failed to configure iptables.\n");
    }
}

// 1. The Cleanup Function: Deletes exactly what we created
void cleanup_os_routing() {
    printf("\n[Server] Cleaning up OS Routing rules...\n");

    // Revert IP Forwarding back to 0
    int fd = open("/proc/sys/net/ipv4/ip_forward", O_WRONLY);
    if (fd >= 0) {
        write(fd, "0\n", 2);
        close(fd);
        printf("[Server] [-] Native IP Forwarding disabled.\n");
    }

    // Delete the NAT rule. Notice we use '-D' (Delete) instead of '-A' (Append)
    // We add 2>/dev/null to hide errors if the rule was already deleted
    int ret = system("iptables -t nat -D POSTROUTING -s 10.0.0.0/24 -o eth0 -j MASQUERADE 2>/dev/null");
    if (ret == 0) {
        printf("[Server] [-] NAT Masquerading removed from iptables.\n");
    }

    printf("[Server] Shutdown complete. System restored.\n");
}

// 2. The Signal Catcher
void handle_shutdown(int sig) {
    printf("\n[Server] Caught Signal %d. Initiating graceful shutdown...\n", sig);
    cleanup_os_routing();
    
    // Close the global sockets so the OS frees the ports immediately
    if (global_tun_fd >= 0) close(global_tun_fd);
    if (global_udp_fd >= 0) close(global_udp_fd);
    
    exit(0); // Exit the program cleanly
}


int main(void) {
    signal(SIGINT, handle_shutdown);  // Catches Ctrl+C
    signal(SIGTERM, handle_shutdown);
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

    configure_os_routing();

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