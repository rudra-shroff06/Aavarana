#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define UNIX_SOCK_PATH "/tmp/aavarana_admin.sock"

int main() {
    int sock;
    struct sockaddr_un addr;
    char buffer[4096];
    int bytes_read;


    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("Socket error");
        exit(1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, UNIX_SOCK_PATH, sizeof(addr.sun_path)-1);


    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        fprintf(stderr, "[ERROR] Is the VPN server running? (Socket not found)\n");
        close(sock);
        exit(1);
    }


    printf("\033[1;32m--- Aavarana VPN: Live Session Table ---\033[0m\n");
    while ((bytes_read = read(sock, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }
    printf("\033[1;32m----------------------------------------\033[0m\n");

    close(sock);
    return 0;
}