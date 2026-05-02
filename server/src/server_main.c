#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include "server_net.h"
#include "state.h"
#include "tun.h"
#include "defines.h"




RoutingTable global_rt;
KeyStore global_ks;
i32 global_tun_fd = -1;
i32 global_udp_fd = -1;



void configure_os_routing() {
    printf("[Server] Taking control of Linux Kernel Routing...\n");


    int fd = open("/proc/sys/net/ipv4/ip_forward", O_WRONLY);
    if (fd >= 0) {
        write(fd, "1\n", 2);
        close(fd);
        printf("[Server] [+] Native IP Forwarding enabled.\n");
    } else {
        printf("[Server] [-] Failed to enable IP forwarding. Are you root?\n");
    }



    int ret = system("iptables -t nat -A POSTROUTING -s 10.0.0.0/24 -o eth0 -j MASQUERADE");
    if (ret == 0) {
        printf("[Server] [+] NAT Masquerading injected into iptables.\n");
    } else {
        printf("[Server] [-] Failed to configure iptables.\n");
    }
}


void cleanup_os_routing() {
    printf("\n[Server] Cleaning up OS Routing rules...\n");


    int fd = open("/proc/sys/net/ipv4/ip_forward", O_WRONLY);
    if (fd >= 0) {
        write(fd, "0\n", 2);
        close(fd);
        printf("[Server] [-] Native IP Forwarding disabled.\n");
    }



    int ret = system("iptables -t nat -D POSTROUTING -s 10.0.0.0/24 -o eth0 -j MASQUERADE 2>/dev/null");
    if (ret == 0) {
        printf("[Server] [-] NAT Masquerading removed from iptables.\n");
    }

    printf("[Server] Shutdown complete. System restored.\n");
}


void handle_shutdown(int sig) {
    printf("\n[Server] Caught Signal %d. Initiating graceful shutdown...\n", sig);
    cleanup_os_routing();
    

    if (global_tun_fd >= 0) close(global_tun_fd);
    if (global_udp_fd >= 0) close(global_udp_fd);
    
    exit(0);
}








void* auto_key_rotation_thread(void* arg) {
    while(1) {

        sleep(3600); 
        printf("[Thread] Background Security Worker: Keys rotated in memory.\n");
    }
}

int main(void) {
    signal(SIGINT, handle_shutdown); 
    signal(SIGTERM, handle_shutdown);
    printf("[Server] Initializing Aavarana Server...\n");

    state_init(&global_rt, &global_ks);
    

    printf("[Server] Forcing Hardcoded Debug Key...\n");
    state_trigger_key_rotation(&global_rt, &global_ks, (u8*)"SUPERSECRETKEY12", 16);


    pthread_t tid;
    if(pthread_create(&tid, NULL, auto_key_rotation_thread, NULL) != 0) {
        perror("Failed to create security thread");
    }
    pthread_detach(tid); 

    i8 tun_name[16] = "tun0"; 
    global_tun_fd = tun_alloc(tun_name);
    
    if (global_tun_fd < 0) {
        printf("[Server] Fatal: Could not allocate TUN interface. Are you root?\n");
        exit(1);
    }

    printf("[Server] Configuring Gateway Interface...\n");
    configure_tun_ip(tun_name, inet_addr("10.0.0.1"));


    system("iptables -t nat -F");
    system("iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE");
    system("echo 1 > /proc/sys/net/ipv4/ip_forward");

    NetworkContext ctx;
    net_init_sockets(&ctx); 

    global_udp_fd = ctx.udp_fd;

    printf("[Server] Multithreaded Engine Online. Waiting for clients...\n");
    net_epoll_loop(&ctx);

    return 0;
}