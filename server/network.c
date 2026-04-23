#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/ip.h>
#include "../defines.h"


volatile int fd_closed = -1;

void handle_close(int sigint) {
    if(fd_closed == -1) return;
    close(fd_closed);
    exit(1);
}




int main() {
    // SETUP
    signal(SIGUSR1, handle_close);
    int sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in my_addr;
    socklen_t my_addr_sock_len = sizeof(my_addr);
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(SERVER_PORT);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    fd_closed = sock_fd;

    if(bind(sock_fd, (struct sockaddr*)&my_addr, my_addr_sock_len) == -1) {
        perror("[ERROR]");
        close(sock_fd);
        exit(1);
    }

    char buffer_send[] = "PONG";
    char buffer_recv[10];
    while(1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_sock_len;
        recvfrom(sock_fd, buffer_recv, sizeof(buffer_recv), 0, (struct sockaddr*)&client_addr, &client_addr_sock_len);
        printf("Received Ping...\n");
        sendto(sock_fd, buffer_send, sizeof(buffer_send), 0, (struct sockaddr*)&client_addr, client_addr_sock_len);
    }

    close(sock_fd);


    return 0;
}