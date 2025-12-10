#ifndef UI_HANDLER_H
#define UI_HANDLER_H

#include "client_core.h"
#include <stdbool.h>

// ============================================
// Конфигурация UI
// ============================================

#define UI_MAX_INPUT_LENGTH 512
#define UI_MAX_HISTORY 100
#define UI_PROMPT "> "

// ============================================
// Команды пользователя
// ============================================

typedef enum {
    CMD_UNKNOWN = 0,
    CMD_HELP,           // /help
    CMD_LOGIN,          // /login <username>
    CMD_LOGOUT,         // /logout
    CMD_SEND,           // /send <user> <message>  или  <user> <message>
    CMD_BROADCAST,      // /broadcast <message>
    CMD_USERS,          // /users
    CMD_STATUS,         // /status <online|away|busy|offline>
    CMD_EXIT,           // /exit
    CMD_CLEAR,          // /clear
    CMD_HISTORY,        // /history
    CMD_ME              // /me <action>
} UserCommand;

// ============================================
// Структура распарсенной команды
// ============================================

typedef struct {
    UserCommand type;
    char arg1[256];    // Первый аргумент (логин/статус и т.д.)
    char arg2[512];    // Второй аргумент (сообщение)
    char raw_input[UI_MAX_INPUT_LENGTH]; // Исходный ввод
} ParsedCommand;

// ============================================
// Структура UI состояния
// ============================================

typedef struct {
    bool running;
    bool show_timestamps;
    bool show_system_messages;
    bool color_enabled;
    
    // История сообщений
    Message message_history[UI_MAX_HISTORY];
    int history_size;
    int history_index; // Для прокрутки
    
    // Ввод
    char input_buffer[UI_MAX_INPUT_LENGTH];
    int cursor_pos;
    
    // Статистика
    int messages_displayed;
    time_t session_start;
    
} UIState;

// ============================================
// Основной API UI
// ============================================

// Инициализация/очистка
UIState* ui_create(void);
void ui_destroy(UIState *ui);

// Основной цикл
void ui_main_loop(Client *client);
void ui_stop_loop(UIState *ui);

// Обработка ввода
int ui_read_input(UIState *ui);
ParsedCommand ui_parse_command(const char *input);
void ui_execute_command(Client *client, UIState *ui, ParsedCommand cmd);
void ui_process_input(Client *client, UIState *ui, const char *input);

// Вывод
void ui_print_message(UIState *ui, const Message *msg);
void ui_print_system(UIState *ui, const char *format, ...);
void ui_print_error(UIState *ui, const char *format, ...);
void ui_print_success(UIState *ui, const char *format, ...);
void ui_print_info(UIState *ui, const char *format, ...);
void ui_print_prompt(UIState *ui, const Client *client);
void ui_clear_screen(UIState *ui);
void ui_show_welcome(void);
void ui_show_help(UIState *ui);

// Форматирование
void ui_format_message(UIState *ui, const Message *msg, char *buffer, size_t size);
void ui_format_timestamp(time_t timestamp, char *buffer, size_t size);
const char* ui_get_status_icon(UserStatus status);
const char* ui_get_message_type_icon(MessageType type);

// История сообщений
void ui_add_to_history(UIState *ui, const Message *msg);
void ui_show_history(UIState *ui, int count);
void ui_search_history(UIState *ui, const char *search_term);

// Callbacks для клиента
void ui_message_callback(const Message *msg, void *user_data);
void ui_status_callback(ClientState old_state, ClientState new_state, void *user_data);
void ui_error_callback(const char *error_msg, void *user_data);

// Утилиты
void ui_draw_separator(UIState *ui);
void ui_refresh_display(UIState *ui, Client *client);
bool ui_should_exit(UIState *ui);

#endif // UI_HANDLER_H