#include <client.h>

int main(int argc, char *argv[]) {
    ClientConfig config;
    
    // 1. Parse Args & Allocate TUN
    client_bootstrap(argc, (i8**)argv, &config);
    
    // 2. Start the infinite self-healing tunnel loop
    client_run_tunnel(&config);
    
    return 0;
}