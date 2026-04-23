#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <sys/time.h>
#include <errno.h>
#include "../defines.h"








int main() {
    // SETUP
    int sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in my_addr;
    socklen_t my_addr_sock_len = sizeof(my_addr);
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(CLIENT_PORT);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    struct timeval wait_time;
    wait_time.tv_sec = 5;
    wait_time.tv_usec = 0;

    if(setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (void*)&wait_time, sizeof(wait_time)) == -1) {
        perror("[ERROR]");
        close(sock_fd);
        exit(1);
    }

    if(bind(sock_fd, (struct sockaddr*)&my_addr, my_addr_sock_len) == -1) {
        perror("[ERROR]");
        close(sock_fd);
        exit(1);
    }

    struct sockaddr_in server_addr;
    socklen_t server_addr_sock_len = sizeof(server_addr);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(SERVER_IP);


    // BODY
    char buffer_send[] = "PING";
    char buffer_recv[10];
    int ping_reqs;
    int success = 0;
    printf("Enter Ping Requests: ");
    scanf("%d", &ping_reqs);
    for(int i = 1; i <= ping_reqs; i++) {
        printf("Pinging Server Attempt %d\n", i);
        int send_bytes = sendto(sock_fd, buffer_send, sizeof(buffer_send), 0, (struct sockaddr*)&server_addr, server_addr_sock_len);
        printf("Send %d bytes\n", send_bytes);
        int bytes_read = recvfrom(sock_fd, buffer_recv, sizeof(buffer_recv), 0, NULL, NULL);
        if(bytes_read == -1) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("Packet Timeout!\n");
                continue;
            }
            else {
                perror("[ERROR]");
                close(sock_fd);
                exit(1);
            }
        }
        printf("Received Pong\n");
        success++;
        sleep(1);
    }

    printf("Packets %d/%d received\n", success, ping_reqs);

    close(sock_fd);

    return 0;
}