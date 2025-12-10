#ifndef PROTOKOL_H
#define PROTOKOL_H

#include "./message_types.h"
#include <stdint.h>

// ============================================
// Базовые структуры данных
// ============================================

#pragma pack(push, 1)  // Отключаем выравнивание для сетевой передачи

typedef struct {
    MessageType type;
    uint32_t message_id;
    uint32_t timestamp;
    char sender[MAX_LOGIN_LENGTH];
    char receiver[MAX_LOGIN_LENGTH];
    char text[MAX_MESSAGE_LENGTH];
    uint16_t flags;
} Message;


typedef struct {
    char login[MAX_LOGIN_LENGTH];
    UserStatus status;
    uint32_t last_activity;     // время последней активности 
    uint32_t connection_id;     // id соединения (для сервера)
} UserInfo;


typedef struct {
    uint64_t count;
    UserInfo users[MAX_USERS];
} UserList;

typedef struct {
    MessageType original_type;
    uint32_t original_id;
    ErrorCode err_code;
    char info[MAX_MESSAGE_LENGTH];      // доп инфа
    uint32_t data_size;                 // размер доп инфы
                                        // доп данные идут после структуры в памяти
} ServerResponse;

#pragma pack(pop)   // Восстанавливаем выравнивание

// ============================================
// Флаги сообщений (битовые флаги)
// ============================================

#define FLAG_ENCRYPTED     (1 << 0)     // Сообщение зашифровано
#define FLAG_URGENT        (1 << 1)     // Срочное сообщение
#define FLAG_READ_RECEIPT  (1 << 2)     // Требуется подтверждение прочтения
#define FLAG_DELAYED       (1 << 3)     // Отложенная доставка
#define FLAG_OFFLINE_STORE (1 << 4)     // Хранить для offline-пользователя
#define FLAG_SYSTEM        (1 << 5)     // Системное сообщение

// ============================================
// Вспомогательные функции (объявления)
// ============================================

// Проверка корректности логина
int is_valid_login(const char *login);

// Проверка корректности сообщения
int is_valid_message(const Message *msg);

// Получение текущего времени в формате Unix timestamp
uint32_t get_current_timestamp();

// Генерация уникального ID сообщения
uint32_t generate_message_id();

#endif