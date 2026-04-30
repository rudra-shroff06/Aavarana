#include <admin.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>

i32 admin_connect_socket() {
    i32 fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd == -1) {
        perror("[ADMIN]");
        exit(1);
    }

    struct sockaddr_un addr = {.sun_family = AF_UNIX, .sun_path = ADMIN_SOCK_PATH};

    if(connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("[ADMIN]");
        exit(1);
    }

    return fd;

}

void admin_dispatch_command(i32 fd, AdminPayload* payload) {
    if(write(fd, payload, sizeof(AdminPayload)) == -1) {
        perror("[Admin]");
        exit(1);
    }
}

void admin_await_response(i32 fd) {
    u8 status;
    i32 bytes = read(fd, &status, 1);

    if(bytes > 0 && status == 1) {
        printf("[SUCCESS] Command executed by the Server.\n");
    }
    else {
        printf("[FAILED] Server rejected the command or encountered an error.\n");
    }
}