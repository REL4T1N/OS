#ifndef MESSAGE_STORE_H
#define MESSAGE_STORE_H

#include "../common/protocol.h"
#include "../common/utils.h"
#include <pthread.h>
#include <time.h>

// ============================================
// Конфигурация
// ============================================

#define MAX_PENDING_MESSAGES 10000
#define MESSAGE_STORE_FILENAME "offline_messages.dat"

// ============================================
// Структуры
// ============================================

typedef struct PendingMessageNode{
    Message message;
    time_t delivery_time;               // время планируемой доставки
    int delivery_attempts;              // кол-во попыток доставки
    struct PendingMessageNode *next;    // следующая нода
} PendingMessageNode;

typedef struct MessageStore {
    PendingMessageNode *pending_messages;    // список отложенных сообщений
    char *storage_filename;                 // имя файла для сохранения
    pthread_mutex_t mutex;                  
    int message_count;
} MessageStore;

// ============================================
// Основные функции
// ============================================

MessageStore* message_store_create(const char *filename);
void message_store_destroy(MessageStore *store);

int message_store_add(MessageStore *store, const Message *message);
int message_store_remove(MessageStore *store, uint32_t message_id);
PendingMessageNode* message_store_get_for_user(MessageStore *store, const char *login);
int message_store_deliver_for_user(MessageStore *store, const char *login, Message **delivered_messages, int *count);

int message_store_save_to_file(MessageStore *store);
int message_store_load_from_file(MessageStore* store);

int message_store_get_count(MessageStore *store);
void message_store_cleanup_old(MessageStore *store, time_t older_than);
void message_store_print_stats(MessageStore *store);

#endif