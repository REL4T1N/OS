#include "client_core.h"
#include "ui_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

static atomic_bool g_shutdown_requested = false;

void signal_handler(int sig) {
    (void)sig;
    printf("\n[CLIENT] Shutdown requested...\n");
    atomic_store(&g_shutdown_requested, true);
}

void print_usage(const char *program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -h, --help              Show this help\n");
    printf("  -s, --server HOST       Server hostname (default: localhost)\n");
    printf("  -p, --port PORT         Server port (default: 5555)\n");
    printf("  -u, --user USERNAME     Auto-login with username\n");
    printf("  -n, --no-color          Disable colored output\n");
    printf("\nExamples:\n");
    printf("  %s\n", program_name);
    printf("  %s -s 192.168.1.100 -p 5555\n", program_name);
    printf("  %s -u alice\n", program_name);
}

int main(int argc, char *argv[]) {
    // Обработка сигналов
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Параметры по умолчанию
    char server_host[256] = "localhost";
    int server_port = 5555;
    char auto_login[64] = "";
    bool use_color = true;
    
    // Разбор аргументов командной строки
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--server") == 0 || strcmp(argv[i], "-s") == 0) {
            if (i + 1 < argc) {
                strncpy(server_host, argv[++i], sizeof(server_host) - 1);
            }
        } else if (strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "-p") == 0) {
            if (i + 1 < argc) {
                server_port = atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "--user") == 0 || strcmp(argv[i], "-u") == 0) {
            if (i + 1 < argc) {
                strncpy(auto_login, argv[++i], sizeof(auto_login) - 1);
            }
        } else if (strcmp(argv[i], "--no-color") == 0 || strcmp(argv[i], "-n") == 0) {
            use_color = false;
        }
    }
    
    printf("=== ZeroMQ Messenger Client ===\n");
    printf("Server: %s:%d\n", server_host, server_port);
    if (auto_login[0]) {
        printf("Auto-login: %s\n", auto_login);
    }
    printf("\n");
    
    // Создаем клиента
    Client *client = client_create(server_host, server_port);
    if (!client) {
        fprintf(stderr, "Failed to create client\n");
        return 1;
    }
    
    // Подключаемся к серверу
    printf("Connecting to server...\n");
    if (client_connect(client) != 0) {
        fprintf(stderr, "Failed to connect to server\n");
        client_destroy(client);
        return 1;
    }
    
    printf("Connected successfully!\n");
    
    // Автологин если указан
    if (auto_login[0] != '\0') {
        printf("Logging in as %s...\n", auto_login);
        if (client_login(client, auto_login) != 0) {
            fprintf(stderr, "Auto-login failed\n");
            // Продолжаем без логина
        } else {
            printf("Logged in as %s\n", auto_login);
        }
    }
    
    printf("\n");
    printf("Type /help for commands\n");
    printf("Type /login <username> to login\n");
    printf("\n");
    
    // Запускаем клиент
    if (client_start(client) != 0) {
        fprintf(stderr, "Failed to start client\n");
        client_destroy(client);
        return 1;
    }
    
    // Запускаем UI
    ui_main_loop(client);
    
    // Останавливаем клиент
    printf("Stopping client...\n");
    client_stop(client);
    client_disconnect(client);
    client_destroy(client);
    
    printf("Client terminated\n");
    return 0;
}