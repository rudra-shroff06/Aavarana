CC = gcc
CFLAGS = -Wall -Wextra -pthread -g

# Include Directories (This is why <state.h> works)
INCLUDES = -Icommon/include -Iserver/include -Iclient/include -Iadmin/include

# Final Binaries
SERVER_BIN = aavarana_server
CLIENT_BIN = aavarana_client
ADMIN_BIN  = aavarana_admin

# Source Files
COMMON_SRC = common/src/tun.c common/src/crypto.c
SERVER_SRC = server/src/server_main.c server/src/server_net.c server/src/auth_daemon.c server/src/state.c
CLIENT_SRC = client/src/client_main.c client/src/client.c
ADMIN_SRC  = admin/src/admin_main.c admin/src/admin.c

# Object Files (Stored in a build folder)
COMMON_OBJ = $(COMMON_SRC:%.c=build/%.o)
SERVER_OBJ = $(SERVER_SRC:%.c=build/%.o)
CLIENT_OBJ = $(CLIENT_SRC:%.c=build/%.o)
ADMIN_OBJ  = $(ADMIN_SRC:%.c=build/%.o)

all: $(SERVER_BIN) $(CLIENT_BIN) $(ADMIN_BIN)

# Build Server
$(SERVER_BIN): $(SERVER_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

# Build Client
$(CLIENT_BIN): $(CLIENT_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

# Build Admin CLI
$(ADMIN_BIN): $(ADMIN_OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

# Rule for Object Files
build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf build $(SERVER_BIN) $(CLIENT_BIN) $(ADMIN_BIN)

.PHONY: all clean