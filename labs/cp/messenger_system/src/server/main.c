#include "./server_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    printf("=== ZeroMQ Messenger Server ===\n");
    
    const char *bind_address = "*";
    int port = 5555;
    
    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("Usage: %s [bind_address] [port]\n", argv[0]);
            printf("Default: %s * 5555\n", argv[0]);
            return 0;
        }
        bind_address = argv[1];
    }
    
    if (argc > 2) {
        port = atoi(argv[2]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port: %s\n", argv[2]);
            return 1;
        }
    }
    
    printf("Binding to: %s:%d\n", bind_address, port);
    printf("PUB socket: %s:%d\n", bind_address, SERVER_PUB_PORT);
    printf("PULL socket: %s:%d\n", bind_address, SERVER_PULL_PORT);
    printf("Press Ctrl+C to stop\n\n");
    
    Server *server = create_server(bind_address, port);
    if (!server) {
        fprintf(stderr, "Failed to create server\n");
        return 1;
    }
    
    int result = run_server(server);
    destroy_server(server);
    
    return result == 0 ? 0 : 1;
}