#ifndef MESSAGE_TYPES_H
#define MESSAGE_TYPES_H

#include <time.h>

// ============================================
// Типы сообщений для протокола обмена
// ============================================

typedef enum {
    // Команды от клиента к серверу
    MSG_TYPE_REGISTER = 1,      // Регистрация нового пользователя
    MSG_TYPE_LOGIN,             // Вход существующего пользователя - ДОБАВЛЯЕМ
    MSG_TYPE_UNREGISTER,        // Выход пользователя
    MSG_TYPE_TEXT_MESSAGE,      // Текстовое сообщение пользователю
    MSG_TYPE_BROADCAST,         // Сообщение всем пользователям
    MSG_TYPE_GET_USERS,         // Запрос списка пользователей
    MSG_TYPE_SET_STATUS,        // Установка статуса
    
    // Ответы от сервера к клиенту
    MSG_TYPE_ACK,               // Подтверждение получения
    MSG_TYPE_ERROR,             // Ошибка выполнения
    MSG_TYPE_USER_LIST,         // Список пользователей
    MSG_TYPE_SYSTEM_MESSAGE,    // Системное сообщение
    
    // Рассылка от сервера всем клиентам
    MSG_TYPE_USER_JOINED,       // Пользователь присоединился
    MSG_TYPE_USER_LEFT,         // Пользователь вышел
    MSG_TYPE_CHAT_MESSAGE       // Чат-сообщение для доставки
} MessageType;

// ============================================
// Статусы пользователей
// ============================================

typedef enum {
    USER_STATUS_OFFLINE = 0,    // Не в сети
    USER_STATUS_ONLINE,         // В сети, активен
    USER_STATUS_AWAY,           // Отошел
    USER_STATUS_BUSY,           // Занят
    USER_STATUS_INVISIBLE       // Невидимый
} UserStatus;

// ============================================
// Коды ошибок
// ============================================

typedef enum {
    ERROR_SUCCESS = 0,          // Успех
    ERROR_UNKNOWN,              // Неизвестная ошибка
    ERROR_INVALID_LOGIN,        // Неверный логин
    ERROR_LOGIN_EXISTS,         // Логин уже существует
    ERROR_LOGIN_NOT_FOUND,      // Логин не найден
    ERROR_USER_OFFLINE,         // Пользователь не в сети
    ERROR_MESSAGE_TOO_LONG,     // Сообщение слишком длинное
    ERROR_SERVER_FULL,          // Сервер переполнен
    ERROR_INVALID_MESSAGE,      // Неверный формат сообщения
    ERROR_NOT_AUTHORIZED,       // Не авторизован
    ERROR_INTERNAL_SERVER       // Внутренняя ошибка сервера
} ErrorCode;

// ============================================
// Константы и лимиты
// ============================================

#define MAX_LOGIN_LENGTH 32     // Максимальная длина логина
#define MAX_MESSAGE_LENGTH 512  // Максимальная длина сообщения
#define MAX_USERS 10000         // Максимальное количество пользователей
#define SERVER_NAME "MessengerServer"
#define PROTOCOL_VERSION 1

#endif // MESSAGE_TYPES_H