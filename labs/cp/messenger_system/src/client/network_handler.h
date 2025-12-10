#ifndef NETWORK_HANDLER_H
#define NETWORK_HANDLER_H

#include "./client_core.h"

// ============================================
// Функции сетевого взаимодействия
// ============================================

// Инициализация сокетов
int network_init(Client *client);
void network_cleanup(Client *client);

// Отправка сообщений
int network_send_request(Client *client, Message *msg, ServerResponse *resp);
int network_send_message(Client *client, Message *msg);
int network_send_heartbeat(Client *client);

// Получение сообщений
int network_receive_message(Client *client, Message *msg, int timeout_ms);
int network_receive_broadcast(Client *client, char *buffer, size_t buffer_size);

// Подписка/отписка
int network_subscribe(Client *client, const char *filter);
int network_unsubscribe(Client *client, const char *filter);

// Утилиты
const char* network_get_last_error(void);
int network_check_connection(Client *client);

#endif // NETWORK_HANDLER_H