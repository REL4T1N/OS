#include "./message_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

// ============================================
// Вспомогательные функции
// ============================================

PendingMessageNode* create_pending_node(const Message *message) {
    PendingMessageNode *node = (PendingMessageNode*)malloc(sizeof(PendingMessageNode));
    if (!node) return NULL;
    
    memcpy(&node->message, message, sizeof(Message));
    node->delivery_time = time(NULL);
    node->delivery_attempts = 0;
    node->next = NULL;
    
    return node;
}

// ============================================
// Основные функции
// ============================================

MessageStore* message_store_create(const char *filename) {
    MessageStore *store = (MessageStore*)malloc(sizeof(MessageStore));
    if (!store) return NULL;
    
    memset(store, 0, sizeof(MessageStore));
    store->pending_messages = NULL;
    store->message_count = 0;
    
    // Сохраняем имя файла
    if (filename) {
        store->storage_filename = my_strdup(filename);
        if (!store->storage_filename) {
            free(store);
            return NULL;
        }
    } else {
        store->storage_filename = my_strdup(MESSAGE_STORE_FILENAME);
        if (!store->storage_filename) {
            free(store);
            return NULL;
        }
    }
    
    if (pthread_mutex_init(&store->mutex, NULL) != 0) {
        free(store->storage_filename);
        free(store);
        return NULL;
    }
    
    // Загружаем сообщения из файла, если он существует
    message_store_load_from_file(store);
    return store;
}

void message_store_destroy(MessageStore *store) {
    if (!store) return;
    
    pthread_mutex_lock(&store->mutex);
    message_store_save_to_file(store);      // Сохраняем сообщения в файл
    
    // Освобождаем все сообщения
    PendingMessageNode *current = store->pending_messages;
    while (current) {
        PendingMessageNode *next = current->next;
        free(current);
        current = next;
    }
    
    pthread_mutex_unlock(&store->mutex);
    pthread_mutex_destroy(&store->mutex);
    free(store->storage_filename);
    free(store);
}

int message_store_add(MessageStore *store, const Message *message) {
    if (!store || !message) return ERROR_INVALID_MESSAGE;
    
    pthread_mutex_lock(&store->mutex);
    // Проверяем лимит сообщений
    if (store->message_count >= MAX_PENDING_MESSAGES) {
        pthread_mutex_unlock(&store->mutex);
        return ERROR_SERVER_FULL;
    }
    
    // Создаем новый узел
    PendingMessageNode *new_node = create_pending_node(message);
    if (!new_node) {
        pthread_mutex_unlock(&store->mutex);
        return ERROR_INTERNAL_SERVER;
    }
    
    // Добавляем в начало списка
    new_node->next = store->pending_messages;
    store->pending_messages = new_node;
    store->message_count++;
    
    pthread_mutex_unlock(&store->mutex);
    return ERROR_SUCCESS;
}

int message_store_remove(MessageStore *store, uint32_t message_id) {
    if (!store) return ERROR_INVALID_MESSAGE;
    
    pthread_mutex_lock(&store->mutex);
    
    PendingMessageNode *prev = NULL;
    PendingMessageNode *current = store->pending_messages;
    
    while (current) {
        if (current->message.message_id == message_id) {
            // Нашли сообщение, удаляем
            if (prev) {
                prev->next = current->next;
            } else {
                store->pending_messages = current->next;
            }
            
            free(current);
            store->message_count--;
            pthread_mutex_unlock(&store->mutex);
            return ERROR_SUCCESS;
        }
        prev = current;
        current = current->next;
    }
    
    pthread_mutex_unlock(&store->mutex);
    return ERROR_LOGIN_NOT_FOUND;  // Используем как "сообщение не найдено"
}

PendingMessageNode* message_store_get_for_user(MessageStore *store, const char *login) {
    if (!store || !login) return NULL;
    
    // Ищем первое сообщение для данного пользователя
    pthread_mutex_lock(&store->mutex);
    PendingMessageNode *result = NULL;
    PendingMessageNode *current = store->pending_messages;
    
    while (current) {
        if (strcmp(current->message.receiver, login) == 0) {
            result = current;
            break;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&store->mutex);
    return result;
}

int message_store_deliver_for_user(MessageStore *store, const char *login, Message **delivered_messages, int *count) {
    if (!store || !login || !delivered_messages || !count) return ERROR_INVALID_MESSAGE;
    
    // Считаем сообщения для данного пользователя
    pthread_mutex_lock(&store->mutex);
    int message_count = 0;
    PendingMessageNode *current = store->pending_messages;
    
    while (current) {
        if (strcmp(current->message.receiver, login) == 0) {
            message_count++;
        }
        current = current->next;
    }
    
    if (message_count == 0) {
        *delivered_messages = NULL;
        *count = 0;
        pthread_mutex_unlock(&store->mutex);
        return ERROR_SUCCESS;
    }
    
    // Выделяем память для сообщений
    Message *messages = (Message*)malloc(message_count * sizeof(Message));
    if (!messages) {
        pthread_mutex_unlock(&store->mutex);
        return ERROR_INTERNAL_SERVER;
    }
    
    // Собираем сообщения и удаляем их из списка
    PendingMessageNode *prev = NULL;
    current = store->pending_messages;
    int index = 0;
    
    while (current) {
        if (strcmp(current->message.receiver, login) == 0) {
            // Копируем сообщение
            memcpy(&messages[index], &current->message, sizeof(Message));
            index++;            
            // Удаляем узел
            PendingMessageNode *to_delete = current;
            if (prev) {
                prev->next = current->next;
            } else {
                store->pending_messages = current->next;
            }
    
            current = current->next;
            free(to_delete);
            store->message_count--;
        } else {
            prev = current;
            current = current->next;
        }
    }
    
    *delivered_messages = messages;
    *count = index;
    pthread_mutex_unlock(&store->mutex);
    return ERROR_SUCCESS;
}

int message_store_save_to_file(MessageStore *store) {
    if (!store || !store->storage_filename) return -1;
    
    FILE *file = fopen(store->storage_filename, "wb");
    if (!file) return -1;
    
    fwrite(&store->message_count, sizeof(int), 1, file);
    PendingMessageNode *current = store->pending_messages;
    while (current) {
        fwrite(&current->message, sizeof(Message), 1, file);
        fwrite(&current->delivery_time, sizeof(time_t), 1, file);
        fwrite(&current->delivery_attempts, sizeof(int), 1, file);
        current = current->next;
    }
    
    fclose(file);
    return 0;
}

int message_store_load_from_file(MessageStore *store) {
    if (!store || !store->storage_filename) return -1;
    
    FILE *file = fopen(store->storage_filename, "rb");
    if (!file) {
        // Файл может не существовать - это нормально
        return 0;
    }
    
    // Читаем количество сообщений
    int message_count;
    if (fread(&message_count, sizeof(int), 1, file) != 1) {
        fclose(file);
        return -1;
    }
    
    // Читаем сообщения
    PendingMessageNode *last = NULL;
    for (int i = 0; i < message_count; i++) {
        PendingMessageNode *node = (PendingMessageNode*)malloc(sizeof(PendingMessageNode));
        if (!node) {
            fclose(file);
            return -1;
        }
        
        if (fread(&node->message, sizeof(Message), 1, file) != 1 ||
            fread(&node->delivery_time, sizeof(time_t), 1, file) != 1 ||
            fread(&node->delivery_attempts, sizeof(int), 1, file) != 1) {
            free(node);
            fclose(file);
            return -1;
        }
        
        node->next = NULL;
        if (last) {
            last->next = node;
        } else {
            store->pending_messages = node;
        }
        last = node;
        store->message_count++;
    }
    fclose(file);
    return 0;
}

int message_store_get_count(MessageStore *store) {
    return store ? store->message_count : 0;
}

void message_store_cleanup_old(MessageStore *store, time_t older_than) {
    if (!store || older_than <= 0) return;
    
    time_t now = time(NULL);
    if (now == (time_t)-1) return;
    
    pthread_mutex_lock(&store->mutex);
    PendingMessageNode *prev = NULL;
    PendingMessageNode *current = store->pending_messages;
    
    while (current) {
        time_t age = now - current->delivery_time;
        if (age > older_than) {
            // Сообщение слишком старое, удаляем
            PendingMessageNode *to_delete = current;
            if (prev) {
                prev->next = current->next;
            } else {
                store->pending_messages = current->next;
            }
            
            current = current->next;
            free(to_delete);
            store->message_count--;
        } else {
            prev = current;
            current = current->next;
        }
    }    
    pthread_mutex_unlock(&store->mutex);
}

void message_store_print_stats(MessageStore *store) {
    if (!store) {
        printf("Message store: NULL\n");
        return;
    }
    
    pthread_mutex_lock(&store->mutex);
    printf("Message store: %d pending messages\n", store->message_count);
    pthread_mutex_unlock(&store->mutex);
}