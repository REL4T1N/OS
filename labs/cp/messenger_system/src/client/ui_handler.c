#include "ui_handler.h"
#include "../common/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>

#include <strings.h>

#include <sys/select.h>
#include <sys/time.h>

// ============================================
// Вспомогательные функции
// ============================================

static const char* command_to_string(UserCommand cmd) {
    switch (cmd) {
        case CMD_HELP:      return "HELP";
        case CMD_LOGIN:     return "LOGIN";
        case CMD_LOGOUT:    return "LOGOUT";
        case CMD_SEND:      return "SEND";
        case CMD_BROADCAST: return "BROADCAST";
        case CMD_USERS:     return "USERS";
        case CMD_STATUS:    return "STATUS";
        case CMD_EXIT:      return "EXIT";
        case CMD_CLEAR:     return "CLEAR";
        case CMD_HISTORY:   return "HISTORY";
        case CMD_ME:        return "ME";
        default:            return "UNKNOWN";
    }
}

static UserCommand string_to_command(const char *str) {
    if (!str || str[0] == '\0') return CMD_UNKNOWN;
    
    // Пропускаем '/' если есть
    const char *cmd = str;
    if (cmd[0] == '/') cmd++;
    
    if (strcmp(cmd, "help") == 0) return CMD_HELP;
    if (strcmp(cmd, "login") == 0) return CMD_LOGIN;
    if (strcmp(cmd, "logout") == 0 || strcmp(cmd, "quit") == 0) return CMD_LOGOUT;
    if (strcmp(cmd, "send") == 0 || strcmp(cmd, "msg") == 0) return CMD_SEND;
    if (strcmp(cmd, "broadcast") == 0 || strcmp(cmd, "shout") == 0) return CMD_BROADCAST;
    if (strcmp(cmd, "users") == 0 || strcmp(cmd, "who") == 0) return CMD_USERS;
    if (strcmp(cmd, "status") == 0) return CMD_STATUS;
    if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "bye") == 0) return CMD_EXIT;
    if (strcmp(cmd, "clear") == 0 || strcmp(cmd, "cls") == 0) return CMD_CLEAR;
    if (strcmp(cmd, "history") == 0) return CMD_HISTORY;
    if (strcmp(cmd, "me") == 0) return CMD_ME;
    
    return CMD_UNKNOWN;
}

// ============================================
// Создание/уничтожение UI
// ============================================

UIState* ui_create(void) {
    UIState *ui = (UIState*)calloc(1, sizeof(UIState));
    if (!ui) return NULL;
    
    ui->running = true;
    ui->show_timestamps = true;
    ui->show_system_messages = true;
    ui->color_enabled = true;
    
    ui->history_size = 0;
    ui->history_index = 0;
    ui->messages_displayed = 0;
    ui->session_start = time(NULL);
    
    return ui;
}

void ui_destroy(UIState *ui) {
    if (!ui) return;
    free(ui);
}

// ============================================
// Основной цикл UI
// ============================================

void ui_main_loop(Client *client) {
    if (!client) return;
    
    UIState *ui = ui_create();
    if (!ui) {
        fprintf(stderr, "Failed to create UI\n");
        return;
    }
    
    // Устанавливаем callbacks для клиента
    client_set_message_callback(client, ui_message_callback, ui);
    client_set_status_callback(client, ui_status_callback, ui);
    client_set_error_callback(client, ui_error_callback, ui);
    
    // Приветствие
    ui_show_welcome();
    ui_print_info(ui, "Type /help for commands");
    ui_print_info(ui, "Connect to server with: /login <username>");
    
    char input[UI_MAX_INPUT_LENGTH];
    
    while (ui->running && client_is_connected(client)) {
        // Проверяем входящие сообщения
        ui_refresh_display(ui, client);
        
        // Используем select для неблокирующего чтения stdin
        fd_set fds;
        struct timeval tv;
        
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms
        
        int ready = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
        
        if (ready > 0 && FD_ISSET(STDIN_FILENO, &fds)) {
            // Есть ввод от пользователя
            ui_print_prompt(ui, client);
            
            if (fgets(input, sizeof(input), stdin) == NULL) {
                if (feof(stdin)) {
                    ui_print_system(ui, "EOF detected, exiting...");
                    break;
                }
                continue;
            }
            
            // Убираем перевод строки
            input[strcspn(input, "\n")] = '\0';
            
            // Пропускаем пустые строки
            if (input[0] == '\0') {
                continue;
            }
            
            // Обрабатываем команду
            ui_process_input(client, ui, input);
        } else if (ready == -1) {
            // Ошибка select
            break;
        }
        // ready == 0 - таймаут, продолжаем цикл
    }
    
    // Завершение
    ui_print_system(ui, "Goodbye!");
    ui_destroy(ui);
}

void ui_stop_loop(UIState *ui) {
    if (ui) {
        ui->running = false;
    }
}

// ============================================
// Обработка ввода
// ============================================

int ui_read_input(UIState *ui) {
    if (!ui) return -1;
    
    printf("%s", UI_PROMPT);
    fflush(stdout);
    
    if (fgets(ui->input_buffer, sizeof(ui->input_buffer), stdin) == NULL) {
        return -1;
    }
    
    // Убираем перевод строки
    ui->input_buffer[strcspn(ui->input_buffer, "\n")] = '\0';
    ui->cursor_pos = strlen(ui->input_buffer);
    
    return ui->cursor_pos;
}

ParsedCommand ui_parse_command(const char *input) {
    ParsedCommand cmd = {0};
    
    if (!input || input[0] == '\0') {
        return cmd;
    }
    
    safe_strcpy(cmd.raw_input, input, sizeof(cmd.raw_input));
    
    // Проверяем, является ли это командой (начинается с '/')
    if (input[0] == '/') {
        // Это команда вида "/command arg1 arg2"
        char command[64];
        char rest[UI_MAX_INPUT_LENGTH];
        
        // Разделяем команду и аргументы
        int scan_count = sscanf(input, "/%63s %511[^\n]", command, rest);
        
        if (scan_count >= 1) {
            cmd.type = string_to_command(command);
            
            if (scan_count >= 2) {
                // Парсим аргументы в зависимости от команды
                switch (cmd.type) {
                    case CMD_SEND:
                        // /send user message
                        sscanf(rest, "%255s %511[^\n]", cmd.arg1, cmd.arg2);
                        break;
                        
                    case CMD_LOGIN:
                    case CMD_STATUS:
                        // /login username  или /status online
                        safe_strcpy(cmd.arg1, rest, sizeof(cmd.arg1));
                        break;
                        
                    case CMD_BROADCAST:
                    case CMD_ME:
                        // /broadcast message  или /me action
                        safe_strcpy(cmd.arg2, rest, sizeof(cmd.arg2));
                        break;
                        
                    default:
                        // Для остальных команд берем первый аргумент
                        sscanf(rest, "%255s", cmd.arg1);
                        break;
                }
            }
        } else {
            // Просто "/" или "/command" без аргументов
            cmd.type = string_to_command(input + 1);
        }
    } else {
        // Это сообщение в формате "user message"
        // Проверяем, есть ли пробел
        const char *space = strchr(input, ' ');
        if (space) {
            // Извлекаем получателя
            size_t user_len = space - input;
            if (user_len < sizeof(cmd.arg1)) {
                strncpy(cmd.arg1, input, user_len);
                cmd.arg1[user_len] = '\0';
                
                // Остальное - сообщение
                safe_strcpy(cmd.arg2, space + 1, sizeof(cmd.arg2));
                cmd.type = CMD_SEND;
            }
        } else {
            // Только получатель, без сообщения
            safe_strcpy(cmd.arg1, input, sizeof(cmd.arg1));
            cmd.type = CMD_SEND;
        }
    }
    
    return cmd;
}

void ui_execute_command(Client *client, UIState *ui, ParsedCommand cmd) {
    if (!client || !ui) return;
    
    switch (cmd.type) {
        case CMD_HELP:
            ui_show_help(ui);
            break;
            
        case CMD_LOGIN:
            if (cmd.arg1[0] != '\0') {
                ui_print_info(ui, "Logging in as %s...", cmd.arg1);
                int result = client_login(client, cmd.arg1);
                if (result == ERROR_SUCCESS) {
                    ui_print_success(ui, "Logged in as %s", cmd.arg1);
                } else {
                    ui_print_error(ui, "Login failed: error %d", result);
                }
            } else {
                ui_print_error(ui, "Usage: /login <username>");
            }
            break;
            
        case CMD_LOGOUT:
            ui_print_info(ui, "Logging out...");
            client_logout(client);
            break;
            
        case CMD_SEND:
            if (cmd.arg1[0] != '\0' && cmd.arg2[0] != '\0') {
                ui_print_info(ui, "Sending to %s: %s", cmd.arg1, cmd.arg2);
                int result = client_send_message(client, cmd.arg1, cmd.arg2);
                if (result == ERROR_SUCCESS) {
                    ui_print_success(ui, "Message sent to %s", cmd.arg1);
                } else {
                    ui_print_error(ui, "Failed to send: error %d", result);
                }
            } else {
                ui_print_error(ui, "Usage: /send <user> <message>  or  <user> <message>");
            }
            break;
            
        case CMD_BROADCAST:
            if (cmd.arg2[0] != '\0') {
                ui_print_info(ui, "Broadcasting: %s", cmd.arg2);
                client_send_broadcast(client, cmd.arg2);
            } else {
                ui_print_error(ui, "Usage: /broadcast <message>");
            }
            break;
            
        case CMD_USERS:
            ui_print_info(ui, "Requesting user list...");
            client_request_users(client);
            break;
            
        case CMD_STATUS:
            if (cmd.arg1[0] != '\0') {
                UserStatus status = string_to_status(cmd.arg1);
                ui_print_info(ui, "Setting status to: %s", cmd.arg1);
                client_set_status(client, status);
            } else {
                ui_print_error(ui, "Usage: /status <online|away|busy|offline>");
            }
            break;
            
        case CMD_EXIT:
            ui_print_system(ui, "Exiting...");
            ui->running = false;
            break;
            
        case CMD_CLEAR:
            ui_clear_screen(ui);
            break;
            
        case CMD_HISTORY:
            ui_show_history(ui, 10);
            break;
            
        case CMD_UNKNOWN:
            ui_print_error(ui, "Unknown command. Type /help for list of commands.");
            break;
            
        default:
            ui_print_error(ui, "Command not implemented yet");
            break;
    }
}

void ui_process_input(Client *client, UIState *ui, const char *input) {
    if (!client || !ui || !input) return;
    
    ParsedCommand cmd = ui_parse_command(input);
    ui_execute_command(client, ui, cmd);
}

// ============================================
// Вывод сообщений
// ============================================

void ui_print_message(UIState *ui, const Message *msg) {
    if (!ui || !msg) return;
    
    char formatted[1024];
    ui_format_message(ui, msg, formatted, sizeof(formatted));
    
    printf("%s\n", formatted);
    ui->messages_displayed++;
    
    // Добавляем в историю
    ui_add_to_history(ui, msg);
}

void ui_print_system(UIState *ui, const char *format, ...) {
    if (!ui || !format) return;
    
    printf("[SYSTEM] ");
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    
    printf("\n");
}

void ui_print_error(UIState *ui, const char *format, ...) {
    if (!ui || !format) return;
    
    if (ui->color_enabled) {
        printf("\033[1;31m[ERROR]\033[0m ");
    } else {
        printf("[ERROR] ");
    }
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    
    printf("\n");
}

void ui_print_success(UIState *ui, const char *format, ...) {
    if (!ui || !format) return;
    
    if (ui->color_enabled) {
        printf("\033[1;32m[OK]\033[0m ");
    } else {
        printf("[OK] ");
    }
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    
    printf("\n");
}

void ui_print_info(UIState *ui, const char *format, ...) {
    if (!ui || !format) return;
    
    if (ui->color_enabled) {
        printf("\033[1;34m[INFO]\033[0m ");
    } else {
        printf("[INFO] ");
    }
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    
    printf("\n");
}

void ui_print_prompt(UIState *ui, const Client *client) {
    if (!ui) return;
    
    if (ui->color_enabled) {
        printf("\033[1;33m");
    }
    
    if (client && client_is_logged_in((Client*)client)) {
        printf("%s> ", client->login);
    } else {
        printf("not logged in> ");
    }
    
    if (ui->color_enabled) {
        printf("\033[0m");
    }
    
    fflush(stdout);
}

void ui_clear_screen(UIState *ui) {
    (void)ui; // Пока не используем
    printf("\033[2J\033[H"); // ANSI escape codes для очистки экрана
}

void ui_show_welcome(void) {
    printf("=========================================\n");
    printf("    ZeroMQ Messenger Client\n");
    printf("=========================================\n");
    printf("\n");
}

void ui_show_help(UIState *ui) {
    (void)ui; // Пока не используем
    
    printf("\n=== Available Commands ===\n");
    printf("  /help                 - Show this help\n");
    printf("  /login <username>     - Login to server\n");
    printf("  /logout               - Logout from server\n");
    printf("  /send <user> <msg>    - Send private message\n");
    printf("  <user> <msg>          - Shortcut for sending\n");
    printf("  /broadcast <msg>      - Send to all users\n");
    printf("  /users                - Show online users\n");
    printf("  /status <status>      - Set status (online/away/busy)\n");
    printf("  /clear                - Clear screen\n");
    printf("  /history              - Show message history\n");
    printf("  /exit                 - Exit program\n");
    printf("\n");
}

// ============================================
// Callbacks для клиента
// ============================================

void ui_message_callback(const Message *msg, void *user_data) {
    UIState *ui = (UIState*)user_data;
    if (!ui || !msg) return;
    
    ui_print_message(ui, msg);
}

void ui_status_callback(ClientState old_state, ClientState new_state, void *user_data) {
    UIState *ui = (UIState*)user_data;
    if (!ui) return;
    
    const char *old_str = "UNKNOWN";
    const char *new_str = "UNKNOWN";
    
    // Простая конвертация состояний в строки
    switch (old_state) {
        case CLIENT_STATE_DISCONNECTED: old_str = "DISCONNECTED"; break;
        case CLIENT_STATE_CONNECTING:   old_str = "CONNECTING"; break;
        case CLIENT_STATE_CONNECTED:    old_str = "CONNECTED"; break;
        case CLIENT_STATE_LOGGED_IN:    old_str = "LOGGED_IN"; break;
        default: break;
    }
    
    switch (new_state) {
        case CLIENT_STATE_DISCONNECTED: new_str = "DISCONNECTED"; break;
        case CLIENT_STATE_CONNECTING:   new_str = "CONNECTING"; break;
        case CLIENT_STATE_CONNECTED:    new_str = "CONNECTED"; break;
        case CLIENT_STATE_LOGGED_IN:    new_str = "LOGGED_IN"; break;
        default: break;
    }
    
    ui_print_system(ui, "State changed: %s -> %s", old_str, new_str);
}

void ui_error_callback(const char *error_msg, void *user_data) {
    UIState *ui = (UIState*)user_data;
    if (!ui || !error_msg) return;
    
    ui_print_error(ui, "%s", error_msg);
}

// ============================================
// Форматирование
// ============================================

void ui_format_message(UIState *ui, const Message *msg, char *buffer, size_t size) {
    if (!ui || !msg || !buffer || size == 0) {
        if (buffer && size > 0) buffer[0] = '\0';
        return;
    }
    
    char time_str[16] = "";
    if (ui->show_timestamps) {
        ui_format_timestamp(msg->timestamp, time_str, sizeof(time_str));
    }
    
    // Определяем префикс по типу сообщения
    const char *prefix = "";
    if (ui->color_enabled) {
        if (msg->flags & FLAG_SYSTEM) {
            prefix = "\033[1;36m"; // Cyan for system
        } else if (strcmp(msg->sender, "system") == 0) {
            prefix = "\033[1;35m"; // Magenta for system
        } else {
            prefix = "\033[0m"; // Normal
        }
    }
    
    const char *suffix = ui->color_enabled ? "\033[0m" : "";
    
    if (msg->flags & FLAG_SYSTEM || strcmp(msg->sender, "system") == 0) {
        snprintf(buffer, size, "%s[%s] %s%s", prefix, time_str, msg->text, suffix);
    } else {
        snprintf(buffer, size, "%s[%s] <%s>: %s%s", 
                prefix, time_str, msg->sender, msg->text, suffix);
    }
    
    buffer[size - 1] = '\0';
}

void ui_format_timestamp(time_t timestamp, char *buffer, size_t size) {
    if (!buffer || size == 0) return;
    
    struct tm *tm_info = localtime(&timestamp);
    if (!tm_info) {
        buffer[0] = '\0';
        return;
    }
    
    strftime(buffer, size, "%H:%M", tm_info);
}

// ============================================
// История сообщений
// ============================================

void ui_add_to_history(UIState *ui, const Message *msg) {
    if (!ui || !msg) return;
    
    if (ui->history_size < UI_MAX_HISTORY) {
        memcpy(&ui->message_history[ui->history_size], msg, sizeof(Message));
        ui->history_size++;
    } else {
        // Сдвигаем историю
        memmove(&ui->message_history[0], &ui->message_history[1], 
                (UI_MAX_HISTORY - 1) * sizeof(Message));
        memcpy(&ui->message_history[UI_MAX_HISTORY - 1], msg, sizeof(Message));
    }
}

void ui_show_history(UIState *ui, int count) {
    if (!ui || ui->history_size == 0) {
        ui_print_info(ui, "No message history");
        return;
    }
    
    if (count > ui->history_size) {
        count = ui->history_size;
    }
    
    ui_print_info(ui, "=== Last %d messages ===", count);
    
    int start = ui->history_size - count;
    for (int i = start; i < ui->history_size; i++) {
        ui_print_message(ui, &ui->message_history[i]);
    }
}

// ============================================
// Утилиты
// ============================================

bool ui_should_exit(UIState *ui) {
    return ui ? !ui->running : true;
}

void ui_refresh_display(UIState *ui, Client *client) {
    if (!ui || !client) return;
    
    // Пытаемся получить сообщения каждые 100ms
    Message incoming;
    memset(&incoming, 0, sizeof(Message));
    
    int rc = network_receive_message(client, &incoming, 0);
    
    if (rc == 0) {
        // Получено сообщение
        ui_print_message(ui, &incoming);
    }
    // rc == 1 - нет данных, rc == -1 - ошибка
}