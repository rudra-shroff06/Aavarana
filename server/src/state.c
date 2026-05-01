#include <state.h>



void state_init(RoutingTable* rt, KeyStore* ks) {
    memset(rt->entries, 0, sizeof(RouteEntry) * MAX_CLIENTS);
    memset(ks->current_session_key, 0, sizeof(u8) * MAX_KEY_LENGTH);
    pthread_mutex_init(&rt->lock, NULL);
    pthread_mutex_init(&ks->lock, NULL);
    ks->key_len = 0;
}

i32 state_add_client(RoutingTable* rt, KeyStore* ks, u32 virtual_ip, struct sockaddr_in* real_addr, u8* initial_key_out, const i8* username) {
    int assigned_index = -1;
    pthread_mutex_lock(&rt->lock);
    pthread_mutex_lock(&ks->lock);
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(rt->entries[i].is_active == 0) {
            rt->entries[i].virtual_ip = virtual_ip;
            rt->entries[i].real_addr = *real_addr;
            rt->entries[i].is_active = 1;
            strncpy(rt->entries[i].username, username, sizeof(rt->entries[i].username) - 1);
            rt->entries[i].username[sizeof(rt->entries[i].username) - 1] = '\0';
            assigned_index = i;
            break;
        }
    }

    if(assigned_index != -1) {
        memcpy(initial_key_out, ks->current_session_key, ks->key_len);
    }

    pthread_mutex_unlock(&ks->lock);
    pthread_mutex_unlock(&rt->lock);
    return assigned_index;
}

void state_remove_client(RoutingTable* rt, u32 virtual_ip) {
    pthread_mutex_lock(&rt->lock);
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(rt->entries[i].virtual_ip == virtual_ip) {
            rt->entries[i].is_active = 0;
            break;
        }
    }
    pthread_mutex_unlock(&rt->lock);

}

void state_trigger_key_rotation(RoutingTable* rt, KeyStore* ks, u8* new_key, u32 new_len) {
    pthread_mutex_lock(&rt->lock);
    pthread_mutex_lock(&ks->lock);

    memcpy(ks->current_session_key, new_key, new_len);
    ks->key_len = new_len;

    pthread_mutex_unlock(&ks->lock);
    pthread_mutex_unlock(&rt->lock);

}