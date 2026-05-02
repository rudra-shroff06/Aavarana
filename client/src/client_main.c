#include <client.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h> 


void cleanup_client_routing() {
    printf("\n[Client] Disconnecting VPN... Restoring normal Wi-Fi routing...\n");
    
    
    system("sudo ip route del 0.0.0.0/1 2>/dev/null");
    system("sudo ip route del 128.0.0.0/1 2>/dev/null");
    
    
    system("sudo resolvectl revert tun0 2>/dev/null");
    
    printf("[Client] Standard internet routing restored. Safe to exit.\n");
}


void handle_client_shutdown(int sig) {
    cleanup_client_routing();
    exit(0);
}

int main(int argc, char *argv[]) {

    signal(SIGINT, handle_client_shutdown);
    signal(SIGTERM, handle_client_shutdown);

    ClientConfig config;
    

    client_bootstrap(argc, (i8**)argv, &config);
    

    client_run_tunnel(&config);
    
    return 0;
}