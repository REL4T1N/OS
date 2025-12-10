#ifndef USER_MANAGER_H
#define USER_MANAGER_H

#include "../common/protocol.h"
#include "../common/utils.h"
#include <pthread.h>
#include <stdatomic.h>

// ============================================
// Конфигурация
// ============================================

#define USER_HASH_TABLE_SIZE 101  // Простое число для хэш-таблицы
#define MAX_USER_SESSIONS 1000

// ============================================
// Структуры
// ============================================

typedef struct UserSession {
    char login[MAX_LOGIN_LENGTH];
    void *client_address;           // адрес клиента для идентификации ZeroMQ
    time_t last_activity;           // последняя активность
    UserStatus status;              // текущий статус пользователя
    uint32_t connection_id;         // ID соединения
    struct UserSession *next;       // linked-list в хэщ-таблице
} UserSession;

typedef struct UserManager {
    UserSession *hash_table[USER_HASH_TABLE_SIZE];  // хэш мапа
    atomic_int user_count;
    atomic_int online_count;
    pthread_mutex_t mutex;
} UserManager;

// ============================================
// Основные функции
// ============================================

UserManager* user_manager_create(void);
void user_manager_destroy(UserManager *manager);

int user_manager_register(UserManager *manager, const char *login, void *client_address, uint32_t connection_id);
int user_manager_unregister(UserManager *manager, const char *login);
int user_manager_login(UserManager *manager, const char *login, void *client_address, uint32_t connection_id);
int user_manager_logout(UserManager *manager, const char *login);
int user_manager_update_status(UserManager *manager, const char *login, UserStatus new_status);

UserSession* user_manager_find(UserManager *manager, const char *login);
int user_manager_is_online(UserManager *manager, const char *login);
int user_manager_exists(UserManager *manager, const char *login);

UserSession** user_manager_get_online_list(UserManager *manager, int *count);
char** user_manager_get_online_logins(UserManager *manager, int *count);

void user_manager_update_activity(UserManager *manager, const char *login);
void user_manager_cleanup_inactive(UserManager *manager, time_t timeout_seconds);
int user_manager_get_user_count(UserManager *manager);
int user_manager_get_online_count(UserManager *manager);

#endif
