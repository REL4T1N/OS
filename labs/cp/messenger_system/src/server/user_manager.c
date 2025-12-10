#include "./user_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

// ============================================
// Вспомогательные функции
// ============================================

// Простая хэш-функция для логина
unsigned int hash_login(const char *login) {
    unsigned int hash = 0;
    while (*login) {
        hash = (hash * 31) + *login;
        login++;
    }
    return hash % USER_HASH_TABLE_SIZE;
}

UserSession* create_user_session(const char *login, void *client_address, uint32_t connection_id) {
    UserSession *session = (UserSession*)malloc(sizeof(UserSession));
    if (!session) return NULL;
    
    memset(session, 0, sizeof(UserSession));
    strncpy(session->login, login, MAX_LOGIN_LENGTH - 1);
    session->login[MAX_LOGIN_LENGTH - 1] = '\0';
    
    session->client_address = client_address;
    session->connection_id = connection_id;
    session->last_activity = time(NULL);
    session->status = USER_STATUS_ONLINE;
    session->next = NULL;
    
    return session;
}

// ============================================
// Основные функции
// ============================================

UserManager* user_manager_create(void) {
    UserManager *manager = (UserManager*)malloc(sizeof(UserManager));
    if (!manager) return NULL;

    memset(manager, 0, sizeof(UserManager));
    atomic_init(&manager->user_count, 0);
    atomic_init(&manager->online_count, 0);

    if (pthread_mutex_init(&manager->mutex, NULL) != 0) {
        free(manager);
        return NULL;
    }
    return manager;
}

void user_manager_destroy(UserManager *manager) {
    if (!manager) return;
    
    pthread_mutex_lock(&manager->mutex);
    for (int i = 0; i < USER_HASH_TABLE_SIZE; i++) {
        UserSession *current = manager->hash_table[i];
        while (current) {
            UserSession *next = current->next;
            free(current);
            current = next;
        }
        manager->hash_table[i] = NULL;
    }

    pthread_mutex_unlock(&manager->mutex);
    pthread_mutex_destroy(&manager->mutex);
    free(manager);
}

int user_manager_register(UserManager *manager, const char *login, void *client_address, uint32_t connection_id) {
    if (!manager || !login || !client_address) return ERROR_INVALID_MESSAGE;
    
    pthread_mutex_lock(&manager->mutex);
    unsigned int hash = hash_login(login);
    UserSession *current = manager->hash_table[hash];
    while (current) {
        if (strcmp(current->login, login) == 0) {
            pthread_mutex_unlock(&manager->mutex);
            return ERROR_LOGIN_EXISTS;
        }
        current = current->next;
    }

    UserSession *new_session = create_user_session(login, client_address, connection_id);
    if (!new_session) {
        pthread_mutex_unlock(&manager->mutex);
        return ERROR_INTERNAL_SERVER;
    }

    new_session->next = manager->hash_table[hash];
    manager->hash_table[hash] = new_session;
    atomic_fetch_add(&manager->user_count, 1);
    atomic_fetch_add(&manager->online_count, 1);
    pthread_mutex_unlock(&manager->mutex);
    return ERROR_SUCCESS;
}

int user_manager_unregister(UserManager *manager, const char *login) {
    if (!manager || !login) return ERROR_INVALID_MESSAGE;

    pthread_mutex_lock(&manager->mutex);
    unsigned int hash = hash_login(login);
    UserSession *prev = NULL;
    UserSession *current = manager->hash_table[hash];
    while (current) {
        if (strcmp(current->login, login) == 0) {
            if (prev) {
                prev->next = current->next;
            } else {
                manager->hash_table[hash] = current->next;
            }
            
            if (current->status != USER_STATUS_OFFLINE) {
                atomic_fetch_sub(&manager->online_count, 1);
            }
            atomic_fetch_sub(&manager->user_count, 1);
            
            free(current);
            pthread_mutex_unlock(&manager->mutex);
            return ERROR_SUCCESS;
        }
        prev = current;
        current = current->next;
    }
    pthread_mutex_unlock(&manager->mutex);
    return ERROR_LOGIN_NOT_FOUND;
}

int user_manager_login(UserManager *manager, const char *login, void *client_address, uint32_t connection_id) {
    if (!manager || !login || !client_address) return ERROR_INVALID_MESSAGE;
    
    pthread_mutex_lock(&manager->mutex);
    UserSession *session = user_manager_find(manager, login);
    if (!session) {
        // Пользователь не найден, регистрируем его
        pthread_mutex_unlock(&manager->mutex);
        return user_manager_register(manager, login, client_address, connection_id);
    }

    // Обновляем данные существующей сессии
    session->client_address = client_address;
    session->connection_id = connection_id;
    session->last_activity = time(NULL);

    // Если был оффлайн, увеличиваем счетчик онлайн
    if (session->status == USER_STATUS_OFFLINE) {
        session->status = USER_STATUS_ONLINE;
        atomic_fetch_add(&manager->online_count, 1);
    }
    pthread_mutex_unlock(&manager->mutex);
    return ERROR_SUCCESS;
}

int user_manager_logout(UserManager *manager, const char *login) {
    if (!manager || !login) return ERROR_INVALID_MESSAGE;
    
    pthread_mutex_lock(&manager->mutex);
    UserSession *session = user_manager_find(manager, login);
    if (!session) {
        pthread_mutex_unlock(&manager->mutex);
        return ERROR_LOGIN_NOT_FOUND;
    }
    
    // Устанавливаем статус оффлайн
    if (session->status != USER_STATUS_OFFLINE) {
        session->status = USER_STATUS_OFFLINE;
        atomic_fetch_sub(&manager->online_count, 1);
    }
    
    // Очищаем адрес клиента
    session->client_address = NULL;
    session->last_activity = time(NULL);
    pthread_mutex_unlock(&manager->mutex);
    return ERROR_SUCCESS; 
}

int user_manager_update_status(UserManager *manager, const char *login, UserStatus new_status) {
    if (!manager || !login) {
        return ERROR_INVALID_MESSAGE;
    }
    
    if (new_status < USER_STATUS_OFFLINE || new_status > USER_STATUS_INVISIBLE) {
        return ERROR_INVALID_MESSAGE;
    }
    
    pthread_mutex_lock(&manager->mutex);
    UserSession *session = user_manager_find(manager, login);
    if (!session) {
        pthread_mutex_unlock(&manager->mutex);
        return ERROR_LOGIN_NOT_FOUND;
    }
    
    // Обновляем счетчики онлайн
    int was_online = (session->status != USER_STATUS_OFFLINE);
    int will_be_online = (new_status != USER_STATUS_OFFLINE);
    if (was_online && !will_be_online) {
        atomic_fetch_sub(&manager->online_count, 1);
    } else if (!was_online && will_be_online) {
        atomic_fetch_add(&manager->online_count, 1);
    }
    
    session->status = new_status;
    session->last_activity = time(NULL);
    pthread_mutex_unlock(&manager->mutex);
    return ERROR_SUCCESS;    
}

UserSession* user_manager_find(UserManager *manager, const char *login) {
    if (!manager || !login) return NULL;
    
    unsigned int hash = hash_login(login);
    UserSession *current = manager->hash_table[hash];
    while (current) {
        if (strcmp(current->login, login) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

int user_manager_is_online(UserManager *manager, const char *login) {
    if (!manager || !login) return 0;
    
    pthread_mutex_lock(&manager->mutex);
    UserSession *session = user_manager_find(manager, login);
    int is_online = (session && session->status != USER_STATUS_OFFLINE);
    pthread_mutex_unlock(&manager->mutex);

    return is_online;    
}

int user_manager_exists(UserManager *manager, const char *login) {
    if (!manager || !login) return 0;
    
    pthread_mutex_lock(&manager->mutex);
    UserSession *session = user_manager_find(manager, login);
    int exists = (session != NULL);
    pthread_mutex_unlock(&manager->mutex);
    
    return exists;
}

UserSession** user_manager_get_online_list(UserManager *manager, int *count) {
    if (!manager || !count) return NULL;
    
    pthread_mutex_lock(&manager->mutex);
    int online = atomic_load(&manager->online_count);
    UserSession **list = (UserSession**)malloc(online * sizeof(UserSession*));
    if (!list) {
        pthread_mutex_unlock(&manager->mutex);
        return NULL;
    }
    
    int index = 0;
    for (int i = 0; i < USER_HASH_TABLE_SIZE; i++) {
        UserSession *current = manager->hash_table[i];
        while (current) {
            if (current->status != USER_STATUS_OFFLINE) {
                list[index++] = current;
            }
            current = current->next;
        }
    }
    *count = index;
    pthread_mutex_unlock(&manager->mutex);
    return list;  
}

char** user_manager_get_online_logins(UserManager *manager, int *count) {
    if (!manager || !count) return NULL;
    
    pthread_mutex_lock(&manager->mutex);
    int online = atomic_load(&manager->online_count);
    char **logins = (char**)malloc(online * sizeof(char*));
    if (!logins) {
        pthread_mutex_unlock(&manager->mutex);
        return NULL;
    }
    
    int index = 0;
    for (int i = 0; i < USER_HASH_TABLE_SIZE; i++) {
        UserSession *current = manager->hash_table[i];
        while (current) {
            if (current->status != USER_STATUS_OFFLINE) {
                logins[index] = my_strdup(current->login);
                if (!logins[index]) {
                    // Освобождаем уже выделенную память при ошибке
                    for (int j = 0; j < index; j++) {
                        free(logins[j]);
                    }
                    free(logins);
                    pthread_mutex_unlock(&manager->mutex);
                    return NULL;
                }
                index++;
            }
            current = current->next;
        }
    }
    *count = index;
    pthread_mutex_unlock(&manager->mutex);
    return logins;   
}

void user_manager_update_activity(UserManager *manager, const char *login) {
    if (!manager || !login) return;
    
    pthread_mutex_lock(&manager->mutex);
    UserSession *session = user_manager_find(manager, login);
    if (session) {
        session->last_activity = time(NULL);
    }
    pthread_mutex_unlock(&manager->mutex);
}

void user_manager_cleanup_inactive(UserManager *manager, time_t timeout_seconds) {
    if (!manager || timeout_seconds <= 0) return;
    
    time_t now = time(NULL);
    if (now == (time_t)-1) return;
    
    pthread_mutex_lock(&manager->mutex);
    for (int i = 0; i < USER_HASH_TABLE_SIZE; i++) {
        UserSession *prev = NULL;
        UserSession *current = manager->hash_table[i];
        while (current) {
            time_t inactive_time = now - current->last_activity;
            if (inactive_time > timeout_seconds) {
                // Удаляем неактивную сессию
                UserSession *to_delete = current;
                if (prev) {
                    prev->next = current->next;
                } else {
                    manager->hash_table[i] = current->next;
                }
                // Обновляем счетчики
                current = current->next;
                if (to_delete->status != USER_STATUS_OFFLINE) {
                    atomic_fetch_sub(&manager->online_count, 1);
                }
                atomic_fetch_sub(&manager->user_count, 1);
                free(to_delete);
            } else {
                prev = current;
                current = current->next;
            }
        }
    }
    pthread_mutex_unlock(&manager->mutex);
}

int user_manager_get_user_count(UserManager *manager) {
    return manager ? atomic_load(&manager->user_count) : 0;
}

int user_manager_get_online_count(UserManager *manager) {
    return manager ? atomic_load(&manager->online_count) : 0;
}