#include <client.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h> // REQUIRED FOR CTRL+C

// 1. The Cleanup Function
void cleanup_client_routing() {
    printf("\n[Client] Disconnecting VPN... Restoring normal Wi-Fi routing...\n");
    
    // Delete the override routes we created
    system("sudo ip route del 0.0.0.0/1 2>/dev/null");
    system("sudo ip route del 128.0.0.0/1 2>/dev/null");
    
    // Restore default DNS (optional, depending on your OS, but good practice)
    system("sudo resolvectl revert tun0 2>/dev/null");
    
    printf("[Client] Standard internet routing restored. Safe to exit.\n");
}

// 2. The Signal Catcher
void handle_client_shutdown(int sig) {
    cleanup_client_routing();
    exit(0);
}

int main(int argc, char *argv[]) {
    // 3. REGISTER THE SIGNAL CATCHER FIRST
    signal(SIGINT, handle_client_shutdown);
    signal(SIGTERM, handle_client_shutdown);

    ClientConfig config;
    
    // 1. Parse Args & Allocate TUN
    client_bootstrap(argc, (i8**)argv, &config);
    
    // 2. Start the infinite self-healing tunnel loop
    client_run_tunnel(&config);
    
    return 0;
}