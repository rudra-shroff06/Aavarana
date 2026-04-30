#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

// Include your admin header which should contain the AdminPayload struct
// and the function prototypes (admin_connect_socket, etc.)
#include "admin.h" 

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Aavarana Control Plane CLI\n");
        printf("Usage:\n");
        printf("  %s rotate\n", argv[0]);
        printf("  %s kick <IP>\n", argv[0]);
        return 1;
    }

    AdminPayload payload;
    memset(&payload, 0, sizeof(AdminPayload));

    // 1. Parse command line arguments
    if (strcmp(argv[1], "rotate") == 0) {
        payload.opcode = CMD_ROTATE_KEYS; // 0x02
        printf("[Admin CLI] Initiating Global Key Rotation...\n");
    } 
    else if (strcmp(argv[1], "kick") == 0 && argc == 3) {
        payload.opcode = CMD_KICK_USER;   // 0x01
        
        // Convert the string IP (e.g., "10.0.0.2") directly into Network Byte Order for the struct
        if (inet_pton(AF_INET, argv[2], &payload.target_ip) != 1) {
            printf("[Admin CLI] Error: Invalid IP address format.\n");
            return 1;
        }
        printf("[Admin CLI] Initiating Kick for IP: %s\n", argv[2]);
    } 
    else {
        printf("[Admin CLI] Invalid command.\n");
        return 1;
    }

    // 2. Execute the Unix Domain Socket flow
    int fd = admin_connect_socket();
    if (fd < 0) {
        // admin_connect_socket should already print an error, but just in case:
        printf("[Admin CLI] Fatal: Could not connect to the Aavarana Server daemon.\n");
        return 1;
    }

    admin_dispatch_command(fd, &payload);
    admin_await_response(fd);
    
    close(fd);
    return 0;
}