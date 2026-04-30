#include <auth_daemon.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h> // for using inet_pton conversion from ip in string to network order ip
#define __USE_POSIX // This macro is defined to access the function fileno required to get the fd of the File opened using the fopen call
#include <stdio.h>


pid_t auth_spawn_worker(i32 pipe_down[2], i32 pipe_up[2]) {
    if(pipe(pipe_down) == -1 || pipe(pipe_up) == -1) {
        perror("[ERROR]");
        return -1;
    }

    pid_t pid = fork();
    if(pid == 0) {
        close(pipe_down[1]);
        close(pipe_up[0]);
    }
    else {
        close(pipe_down[0]);
        close(pipe_up[1]);
    }

    return pid;
}


u8 auth_verify_credentials(const i8* username, const i8* password) {
    FILE* f = fopen("./server/data/users.txt", "r");
    if(!f) {
        return 0;
    }

    i32 fd = fileno(f);

    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    fcntl(fd, F_SETLKW, &lock);

    i8 line[128];
    u8 is_valid = 0;

    while(fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        i8* f_user = strtok(line, ":");
        i8* f_pass = strtok(NULL, ":");

        if(f_user && f_pass) {
            if(strcmp(username, f_user) == 0 && strcmp(password, f_pass) == 0) {
                is_valid = 1;
                break;
            }
        }
    }

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    fclose(f);
    return is_valid;

}


u32 auth_allocate_lease() {
    i32 fd = open("./server/data/leases.txt", O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        perror("Failed to open leases.txt");
        return 0;
    }

    struct flock lock = { .l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0 };
    fcntl(fd, F_SETLKW, &lock);

    FILE *fp = fdopen(fd, "r+");
    if (!fp) {
        lock.l_type = F_UNLCK; fcntl(fd, F_SETLK, &lock); close(fd);
        return 0;
    }

    i8 lines[256][64];
    i32 line_count = 0;
    i32 found_free_index = -1;
    
    u32 subnet_base = ntohl(inet_addr("10.0.0.0"));
    u32 highest_ip_host = subnet_base + 1; // Gateway
    i8 line[64];

    while (fgets(line, sizeof(line), fp)) {
        strcpy(lines[line_count], line);
        
        char ip_str[32]; int status;
        if (sscanf(line, "%31s %d", ip_str, &status) == 2) {
            uint32_t ip_net; inet_pton(AF_INET, ip_str, &ip_net);
            uint32_t ip_host = ntohl(ip_net);
            
            if (ip_host > highest_ip_host) highest_ip_host = ip_host;

            // Mark the first available IP we find
            if (status == 0 && found_free_index == -1) {
                found_free_index = line_count;
            }
        }
        line_count++;
    }

    uint32_t assigned_ip_host = 0;

    // 3. Process the IP in RAM
    if (found_free_index != -1) { // REUSE OLD IP
        char ip_str[32];
        sscanf(lines[found_free_index], "%31s", ip_str); 
        sprintf(lines[found_free_index], "%s 1\n", ip_str); // Update string in RAM
        
        uint32_t net_ip; inet_pton(AF_INET, ip_str, &net_ip);
        assigned_ip_host = ntohl(net_ip);
    } else { // ALLOCATE BRAND NEW IP
        assigned_ip_host = highest_ip_host + 1;
        if (assigned_ip_host <= subnet_base + 254) {
            char new_ip_str[32];
            uint32_t net_ip = htonl(assigned_ip_host);
            inet_ntop(AF_INET, &net_ip, new_ip_str, 32);
            
            sprintf(lines[line_count], "%s 1\n", new_ip_str); // Append to RAM array
            line_count++;
        } else {
            assigned_ip_host = 0; // Subnet full
        }
    }

    // 4. Safely overwrite the file
    if (assigned_ip_host != 0) {
        rewind(fp);
        ftruncate(fd, 0); // WIPE THE OLD FILE DATA
        for (int i = 0; i < line_count; i++) {
            fputs(lines[i], fp);
        }
        fflush(fp);
    }

    // 5. UNLOCK AND CLOSE
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    fclose(fp); // This automatically closes the underlying 'fd' too

    return assigned_ip_host ? htonl(assigned_ip_host) : 0;
}

// -------------------------------------------------------------
// 2. RELEASE LEASE (Also with 100% Multiprocess fcntl Locks)
// -------------------------------------------------------------
void auth_release_lease(uint32_t target_ip_net) {
    int fd = open("./server/data/leases.txt", O_RDWR | O_CREAT, 0644);
    if (fd < 0) return;

    // 1. APPLY THE STRICT WRITE LOCK
    struct flock lock = { .l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0 };
    fcntl(fd, F_SETLKW, &lock);

    FILE *fp = fdopen(fd, "r+");
    if (!fp) {
        lock.l_type = F_UNLCK; fcntl(fd, F_SETLK, &lock); close(fd);
        return;
    }

    char target_ip_str[32];
    inet_ntop(AF_INET, &target_ip_net, target_ip_str, sizeof(target_ip_str));

    // 2. Read entire file into memory
    char lines[256][64];
    int line_count = 0;
    char line[64];

    while (fgets(line, sizeof(line), fp)) {
        char ip_str[32]; int status;
        if (sscanf(line, "%31s %d", ip_str, &status) == 2) {
            if (strcmp(ip_str, target_ip_str) == 0) {
                sprintf(line, "%s 0\n", ip_str); // Modify string in RAM
            }
        }
        strcpy(lines[line_count], line);
        line_count++;
    }

    // 3. Safely overwrite the file
    rewind(fp);
    ftruncate(fd, 0); // WIPE OLD FILE
    for (int i = 0; i < line_count; i++) {
        fputs(lines[i], fp);
    }
    fflush(fp);

    // 4. UNLOCK AND CLOSE
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    fclose(fp);
}