#ifndef TUN_H
#define TUN_H
#include <defines.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#define MTU_SIZE 1400 // in bytes

typedef struct TunConfig {
    u8 dev[16];
    i32 fd;
    i32 mtu_size;
} TunConfig;

void init_tunconfig(struct TunConfig* tconf);
i32 tun_alloc(i8* dev);
ssize_t tun_read(i32 fd, i8* buffer, i32 length);
ssize_t tun_write(i32 fd, i8* buffer, i32 length);
void configure_tun_ip(i8* dev_name, u32 virtual_ip);
#endif // TUN_H