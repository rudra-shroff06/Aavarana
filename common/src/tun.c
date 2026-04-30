#include <tun.h>
#include <stdio.h>



void init_tunconfig(struct TunConfig* tconf) {
    memset(tconf->dev, 0, sizeof(tconf->dev));
    tconf->fd = -1;
    tconf->mtu_size = MTU_SIZE;
}

i32 tun_alloc(i8* dev) {
    
    struct ifreq ifr;
    i32 fd = open("/dev/net/tun", O_RDWR); 
    if(fd == -1) {
        perror("[Error]");
        exit(1);
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_ifru.ifru_flags = IFF_TUN | IFF_NO_PI;
    strncpy(ifr.ifr_ifrn.ifrn_name, dev, IFNAMSIZ);

    if(ioctl(fd, TUNSETIFF, (void*) &ifr) == -1) {
        close(fd);
        perror("[Error]");
        exit(1);
    }

    strcpy(dev, ifr.ifr_ifrn.ifrn_name);
    return fd;
}

ssize_t tun_read(i32 fd, i8* buffer, i32 length) {
    ssize_t nread = read(fd, buffer, length);
    return nread;
}

ssize_t tun_write(i32 fd, i8* buffer, i32 length) {
    ssize_t nwrite = write(fd, buffer, length);
    return nwrite;
}

#include <stdlib.h> // YOU MUST HAVE THIS FOR system()

void configure_tun_ip(i8* dev_name, u32 virtual_ip) {
    i8 ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &virtual_ip, ip_str, INET_ADDRSTRLEN);

    i8 cmd[256];
    
    // BRO, LOOK HERE: sizeof(cmd) is REQUIRED or the compiler will destroy your stack
    snprintf(cmd, sizeof(cmd), "sudo ip addr add %s/24 dev %s", ip_str, dev_name);
    system(cmd);

    snprintf(cmd, sizeof(cmd), "sudo ip link set dev %s up", dev_name);
    system(cmd);

    snprintf(cmd, sizeof(cmd), "sudo ip link set dev %s mtu 1400", dev_name);
    system(cmd);

    printf("[DEBUG] Configured TUN Interface '%s' with IP: %s\n", dev_name, ip_str);
}