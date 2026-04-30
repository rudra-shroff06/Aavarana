#ifndef STATE_H
#define STATE_H
#include <defines.h>
#include <netinet/ip.h>
#include <pthread.h>
#define MAX_CLIENTS 100
#define MAX_KEY_LENGTH 32

typedef struct RouteEntry {
    u32 virtual_ip;
    struct sockaddr_in real_addr;
    u8 is_active;
} RouteEntry;

typedef struct RoutingTable {
    RouteEntry entries[MAX_CLIENTS];
    pthread_mutex_t lock;
} RoutingTable;

typedef struct KeyStore {
    u8 current_session_key[MAX_KEY_LENGTH];
    u32 key_len;
    pthread_mutex_t lock;
} KeyStore;


void state_init(RoutingTable* rt, KeyStore* ks);
i32 state_add_client(RoutingTable* rt, KeyStore* ks, u32 virtual_ip, struct sockaddr_in* real_addr, u8* initial_key_out);
void state_remove_client(RoutingTable* rt, u32 virtual_ip);
void state_trigger_key_rotation(RoutingTable* rt, KeyStore* ks, u8* new_key, u32 new_len);


#endif // STATE_H