#include "./server_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>


// ============================================
// Константы
// ============================================

#define FILENAME_SIZE 256
#define ADDRESS_SIZE 256
#define TIMEOUT_MS 10000
#define HWM_LIMIT 1000

// ============================================
// Внутренние функции
// ============================================

atomic_int global_shutdown_requested = ATOMIC_VAR_INIT(0);

// ЧТО ЗА ПИЗДКЦ В ДВУХ СЛДЕДУЮЩЗИХ ФУКНЦИЯХ НАГЯЪ ОЛНИ СУЩЕСТВУЕТ ЧТО ЗА КРИНЖ СУЩЕСТВО ТЬУПЫЙ ЯЗЫК ПРОГРАММИРОВАНИЯ
void server_signal_handler(int slg) {
    (void)slg;
    atomic_store(&global_shutdown_requested, 1);     // проверка ошибки
}

// /*
// sigaction - функция POSIX для установки обработчиков сигналов
// Принимает: номер сигнала, структуру с обработчиком, старый обработчик (или NULL)
// Возвращает: 0 при успехе, -1 при ошибке
// */
// void server_init_signals(void) {
//     struct sigaction sa;
//     memset(&sa, 0, sizeof(sa));     
//     sa.sa_handler = server_signal_handler;

//     // SIGINT - Ctrl+C, SIGTERM - сигнал завершения
//     if (sigaction(SIGINT, &sa, NULL) == -1) {
//         perror("Failed to set SIGINT handler");
//     }

//     if (sigaction(SIGTERM, &sa, NULL) == -1) {
//         perror("Failed to set SIGTERM handler");
//     }

//     // SIGPIPE - сигнал при записи в закрытый сокет
//     sa.sa_handler = SIG_IGN;
//     if (sigaction(SIGPIPE, &sa, NULL) == -1) {
//         perror("Failed to ignore SIGPIPE");
//     }
// }
void server_init_signals(void) {
    // Используем стандартный signal() вместо sigaction()
    // Это более портативно и не требует сложных макросов
    
    if (signal(SIGINT, server_signal_handler) == SIG_ERR) {
        fprintf(stderr, "Failed to set SIGINT handler\n");
    }
    
    if (signal(SIGTERM, server_signal_handler) == SIG_ERR) {
        fprintf(stderr, "Failed to set SIGTERM handler\n");
    }
    
    // Игнорируем SIGPIPE - если клиент отключится, мы не хотим падать
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        fprintf(stderr, "Failed to ignore SIGPIPE\n");
    }
}

Server *create_server(const char *bind_address, int port) {   
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port: %d\n", port);
        return NULL;
    }

    Server *server = (Server*)malloc(sizeof(Server));
    if (!server) {
        fprintf(stderr, "Failed to allocate server memory\n");
        return NULL;
    }

    // где обработка ошибок
    memset(server, 0, sizeof(Server));             
    atomic_init(&server->running, 0);              
    atomic_init(&server->shutdown_requested, 0);    
    atomic_init(&server->messages_processed, 0);     
    atomic_init(&server->clients_connected, 0);    

    if (bind_address) {
        server->bind_address = my_strdup(bind_address);    
        if (!server->bind_address) {
            perror("Failed to duplicate bind address");
            free(server);
            return NULL;
        }
    } else {
        server->bind_address = my_strdup("*");     
        if (!server->bind_address) {
            perror("Failed to allocate default bind address");
            free(server);
            return NULL;
        }
        /*
        вообще, чем мы блять тут занимаемся, нахуя, чтобы что блять
        */
    }

    server->port = port > 0 ? port : SERVER_DEFAULT_PORT;
    server->start_time = time(NULL);    

    if (pthread_mutex_init(&server->stats_mutex, NULL) != 0) {
        fprintf(stderr, "Failed to initialize mutex\n");
        free(server->bind_address);
        free(server);
        return NULL;
    }

    // самое сложное - создание контекста zeromq для его дальнейшего прокидывания
    server->zmq_context = zmq_ctx_new();    
    if (!server->zmq_context) {
        fprintf(stderr, "Failed to create ZeroMQ context\n");
        pthread_mutex_destroy(&server->stats_mutex);    
        free(server->bind_address);
        free(server);
        return NULL;
    }

    int max_sockets = 3; // rep, pub, pull
    /*
    ZMQ_MAX_SOCKETS - это опция для установки максимального количества сокетов в контексте
    */
    if (zmq_ctx_set(server->zmq_context, ZMQ_MAX_SOCKETS, max_sockets) != 0) {  // что за константа ZMQ_MAX_SOCKETS
        fprintf(stderr, "Failed to set max sockets: %s\n", zmq_strerror(errno));
        zmq_ctx_destroy(server->zmq_context);           
        pthread_mutex_destroy(&server->stats_mutex);    
        free(server->bind_address);
        free(server);
        return NULL;
    }

    server->rep_socket = NULL;
    server->pub_socket = NULL;
    server->pull_socket = NULL;

    // Инициализация менеджера пользователей
    server->user_manager = user_manager_create();
    if (!server->user_manager) {
        fprintf(stderr, "Failed to create user manager\n");
        zmq_ctx_destroy(server->zmq_context);
        pthread_mutex_destroy(&server->stats_mutex);
        free(server->bind_address);
        free(server);
        return NULL;
    }

    // Инициализация хранилища сообщений
    server->message_store = message_store_create(NULL);
    if (!server->message_store) {
        fprintf(stderr, "Failed to create message store\n");
        user_manager_destroy(server->user_manager);
        zmq_ctx_destroy(server->zmq_context);
        pthread_mutex_destroy(&server->stats_mutex);
        free(server->bind_address);
        free(server);
        return NULL;
    }

    char log_filename[FILENAME_SIZE];
    int written = snprintf(log_filename, sizeof(log_filename), "server_%ld.log", (long)server->start_time);
    if (written < 0 || written >= (int)sizeof(log_filename)) {
        fprintf(stderr, "Failed to format log filename\n");
        server->log_file = stderr;
    } else {
        server->log_file = fopen(log_filename, "a");
        if (!server->log_file) {
            perror("Failed to open log file");
            server->log_file = stderr;
        }
    }

    SERVER_LOG_INFO(server, "Server created (bind: %s, port: %d)", server->bind_address, server->port);
    return server;
}

int server_init_sockets(Server *server) {
    if (!server) return -1;
    
    char address[ADDRESS_SIZE];
    int rc;     // rc = result code

    // 1. rep сокет для регистрации и команд
    server->rep_socket = zmq_socket(server->zmq_context, ZMQ_REP);      // что за константа ZMQ_REP
    if (!server->rep_socket) {
        SERVER_LOG_ERROR(server, "Failed to create REP socket: %s", zmq_strerror(errno));        
        return -1;
    }

    /*
    Что за чертовщина дальше происходит?
    */
    int timeout = TIMEOUT_MS;    // таймаут 10с типо для бурной деятельность
    rc = zmq_setsockopt(server->rep_socket, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
    if (rc != 0) {
        SERVER_LOG_ERROR(server, "Failed to set REP socket timeout: %s", zmq_strerror(errno));
        zmq_close(server->rep_socket);
        server->rep_socket = NULL;
        return -1;
    }
    snprintf(address, sizeof(address), "tcp://%s:%d", server->bind_address, server->port);
    rc = zmq_bind(server->rep_socket, address);
    if (rc != 0) {
        SERVER_LOG_ERROR(server, "Failed to bind REP socket to %s: %s", address, zmq_strerror(errno));
        zmq_close(server->rep_socket);
        server->rep_socket = NULL;
        return -1;
    }
    SERVER_LOG_INFO(server, "REP socket bound to %s", address);
    

    // 2. pub сокет для рассылки сообщений
    server->pub_socket = zmq_socket(server->zmq_context, ZMQ_PUB);      // константа ZMQ_PUB        // проверка ошибки
    if (!server->pub_socket) {
        SERVER_LOG_ERROR(server, "Failed to create PUB socket: %s", zmq_strerror(errno));
        zmq_close(server->rep_socket);
        server->rep_socket = NULL;        
        return -1;
    }

    // установим какой-то водяной знак, нахуя только, но ладно
    /*
    HWM (High Water Mark) - ограничение на количество сообщений в очереди
    */
    int hwm = HWM_LIMIT;
    rc = zmq_setsockopt(server->pub_socket, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    if (rc != 0) {
        SERVER_LOG_WARN(server, "Failed to set PUB socket HWM: %s", zmq_strerror(errno));
        // Не фатальная ошибка, продолжаем
    }

    snprintf(address, sizeof(address), "tcp://%s:%d", server->bind_address, SERVER_PUB_PORT);
    rc = zmq_bind(server->pub_socket, address);
    if (rc != 0) {
        SERVER_LOG_ERROR(server, "Failed to bind PUB socket to %s: %s", address, zmq_strerror(errno));
        zmq_close(server->rep_socket);
        zmq_close(server->pub_socket);
        server->rep_socket = NULL;
        server->pub_socket = NULL;
        return -1;
    }

    /*
    Нужно чутка поспать, потому что подписчики могут не успеть подключиться
    Тема часто используется в pub/sub паттернах
    */
    sleep(1);   // проверка ошибки
    SERVER_LOG_INFO(server, "PUB socket bound to %s", address);

    // 3. pull сокет для приёма сообщений от клиентов
    server->pull_socket = zmq_socket(server->zmq_context, ZMQ_PULL);    // константа ZMQ_PULL       // проверка ошибки
    if (!server->pull_socket) {
        SERVER_LOG_ERROR(server, "Failed to create PULL socket: %s", zmq_strerror(errno));
        zmq_close(server->rep_socket);
        zmq_close(server->pub_socket);
        server->rep_socket = NULL;
        server->pub_socket = NULL;        
        return -1;        
    }

    snprintf(address, sizeof(address), "tcp://%s:%d", server->bind_address, SERVER_PULL_PORT);
    rc = zmq_bind(server->pull_socket, address);        // проверка ошибки
    if (rc != 0) {
        SERVER_LOG_ERROR(server, "Failed to bind PULL socket to %s: %s", address, zmq_strerror(errno));
        zmq_close(server->rep_socket);
        zmq_close(server->pub_socket);
        zmq_close(server->pull_socket);
        server->rep_socket = NULL;
        server->pub_socket = NULL;
        server->pull_socket = NULL;        
        return -1;        
    }
    SERVER_LOG_INFO(server, "PULL socket bound to %s", address);
    return 0;
}

void server_close_sockets(Server *server) {
    if (!server) return;

    if (server->rep_socket) {
        if (zmq_close(server->rep_socket) != 0) {
            SERVER_LOG_ERROR(server, "Failed to close REP socket: %s", zmq_strerror(errno));
        }
        server->rep_socket = NULL;
    }
    
    if (server->pub_socket) {
        if (zmq_close(server->pub_socket) != 0) {
            SERVER_LOG_ERROR(server, "Failed to close PUB socket: %s", zmq_strerror(errno));
        }
        server->pub_socket = NULL;   
    }

    if (server->pull_socket) {
        if (zmq_close(server->pull_socket) != 0) {
            SERVER_LOG_ERROR(server, "Failed to close PULL socket: %s", zmq_strerror(errno));
        }
        server->pull_socket = NULL;
    }
}

int run_server(Server *server) {
    if (!server) return -1;

    server_init_signals();
    if (server_init_sockets(server) != 0) {
        SERVER_LOG_ERROR(server, "Failed to initialize sockets");
        return -1;
    }

    SERVER_LOG_INFO(server, "Server starting...");
    atomic_store(&server->running, 1);
    /*
    zmq_poll - функция ZeroMQ для ожидания событий на нескольких сокетах одновременно
    Аналог select()/poll() из Berkeley sockets, но для ZeroMQ
    */
    while (atomic_load(&server->running) && !atomic_load(&global_shutdown_requested)) {
        zmq_pollitem_t items[] = {
            {server->rep_socket, 0, ZMQ_POLLIN, 0},     // Слушаем REP сокет
            {server->pull_socket, 0, ZMQ_POLLIN, 0}     // Слушаем PULL сокет
        };

        int rc = zmq_poll(items, 2, SERVER_TIMEOUT_MS);
        if (rc == -1) {
            if (errno == EINTR) {
                continue;   // тут чё-та скипаем
            }
            SERVER_LOG_ERROR(server, "Poll error: %s", zmq_strerror(errno));
            break;
        }

        // обработка rep сокета
        if (items[0].revents & ZMQ_POLLIN) {
            printf("[SERVER DEBUG] REP socket has data!\n");
            Message msg;
            memset(&msg, 0, sizeof(Message));
            
            int bytes = zmq_recv(server->rep_socket, &msg, sizeof(Message), 0);
            printf("[SERVER DEBUG] Received %d bytes, expected %zu\n", bytes, sizeof(Message));
            if (bytes == sizeof(Message)) {
                printf("[SERVER DEBUG] Message type: %d, sender: %s\n", msg.type, msg.sender);
                atomic_fetch_add(&server->messages_processed, 1);
                server_process_message(server, &msg, server->rep_socket);
                server_send_response(server, server->rep_socket, msg.type, msg.message_id, ERROR_SUCCESS, "OK");
            } else if (bytes == -1) {
                SERVER_LOG_ERROR(server, "Failed to receive message: %s", zmq_strerror(errno));
            } else {
                SERVER_LOG_ERROR(server, "Received incomplete message: %d bytes", bytes);
            }
        }

        // обработка pull сокета
        if (items[1].revents & ZMQ_POLLIN) {
            Message msg;
            memset(&msg, 0, sizeof(Message));

            int bytes = zmq_recv(server->pull_socket, &msg, sizeof(Message), 0);
            if (bytes == sizeof(Message)) {
                atomic_fetch_add(&server->messages_processed, 1);
                server_process_message(server, &msg, NULL);
            }
        }

        // Очистка неактивных пользователей и вывод статистики раз в минуту
        time_t now = time(NULL);
        static time_t last_cleanup = 0;     // static чтобы сохранялось между итерациями
        static time_t last_stats = 0;       // static чтобы сохранялось между итерациями

        if (now - last_cleanup >= 60) {
            last_cleanup = now;
            user_manager_cleanup_inactive(server->user_manager, INACTIVE_TIMEOUT);
            message_store_cleanup_old(server->message_store, 604800); // 7 дней
        }

        if (now - last_stats >= 60) {
            last_stats = now;
            unsigned long long msgs = atomic_load(&server->messages_processed);
            SERVER_LOG_INFO(server, "Stats: %llu messages processed", msgs);
        }
    }
    SERVER_LOG_INFO(server, "Server stopping...");
    
    // cбрасываем флаг running
    atomic_store(&server->running, 0);
    return 0;
}

void stop_server(Server *server) {
    if (server) {
        atomic_store(&server->running, 0);
    }
}

int server_is_running(Server *server) {
    return server ? atomic_load(&server->running) : 0;
}

void destroy_server(Server *server) {
    if (!server) return;

    SERVER_LOG_INFO(server, "Server destroying...");
    if (atomic_load(&server->running)) {
        stop_server(server);
        sleep(1);       // время на обработку
    }

    server_close_sockets(server);

    // удалить контекст ZeroMQ
    if (server->zmq_context) {
        if (zmq_ctx_destroy(server->zmq_context) != 0) {
            fprintf(stderr, "Warning: Failed to destroy ZeroMQ context: %s\n", zmq_strerror(errno));
        }
        server->zmq_context = NULL;
    }

    // Уничтожаем хранилище сообщений
    if (server->message_store) {
        message_store_destroy(server->message_store);
        server->message_store = NULL;
    }

    // Уничтожаем менеджер пользователей
    if (server->user_manager) {
        user_manager_destroy(server->user_manager);
        server->user_manager = NULL;
    }

    // удалить мьютекс
    if (pthread_mutex_destroy(&server->stats_mutex) != 0) {
        fprintf(stderr, "Warning: Failed to destroy mutex: %s\n", strerror(errno));
    }

    if (server->log_file && server->log_file != stderr) {
        if (fclose(server->log_file) != 0) {
            fprintf(stderr, "Warning: Failed to close log file: %s\n", strerror(errno));
        }
    }

    free(server->bind_address);
    free(server);
    printf("Server destroyed\n");  
}

// ============================================
// Обработка сообщений (заглушка)
// ============================================

void server_process_message(Server *server, Message *msg, void *response_socket) {
    // response_socket = server->rep_socket для REP сокета, NULL для PULL сокета
    // Если response_socket != NULL - нужно отправить ответ
    
    if (!server || !msg) return;
    
    int result = ERROR_UNKNOWN;
    int should_send_response = (response_socket != NULL);
    
    SERVER_LOG_DEBUG(server, "Processing %s message type %d from %s", 
                    response_socket ? "REQ/REP" : "PUSH/PULL",
                    msg->type, msg->sender);
    
    switch (msg->type) {
        case MSG_TYPE_REGISTER:
            result = server_handle_register(server, msg, response_socket);
            should_send_response = 0;  // Обработчик уже отправил ответ если нужно
            break;
            
        case MSG_TYPE_LOGIN:
            result = server_handle_login(server, msg, response_socket);
            should_send_response = 0;  // Обработчик уже отправил ответ если нужно
            break;
            
        case MSG_TYPE_GET_USERS:
            result = server_handle_get_users(server, msg, response_socket);
            should_send_response = 0;  // Обработчик уже отправил ответ
            break;
            
        case MSG_TYPE_UNREGISTER:
            // UNREGISTER может приходить через оба сокета
            result = server_handle_logout(server, msg);
            break;
            
        case MSG_TYPE_TEXT_MESSAGE:
            // TEXT_MESSAGE обычно через PUSH/PULL, но может быть и через REP
            result = server_handle_text_message(server, msg);
            break;
            
        case MSG_TYPE_SET_STATUS:
            result = server_handle_set_status(server, msg);
            break;
            
        case MSG_TYPE_BROADCAST:
            // Рассылка всем - через PUSH/PULL
            result = server_handle_broadcast(server, msg);
            break;
            
        default:
            SERVER_LOG_ERROR(server, "Unknown message type: %d", msg->type);
            result = ERROR_INVALID_MESSAGE;
            break;
    }
    
    // Отправляем стандартный ответ только если нужно и есть куда
    if (should_send_response && response_socket) {
        server_send_response(server, response_socket,
                           msg->type, msg->message_id,
                           result, 
                           result == ERROR_SUCCESS ? "OK" : "Error");
    }
    
    SERVER_LOG_DEBUG(server, "Message processed, result: %d", result);
}

int server_handle_broadcast(Server *server, Message *msg) {
    if (!server || !msg) {
        return ERROR_INVALID_MESSAGE;
    }
    
    if (!user_manager_exists(server->user_manager, msg->sender)) {
        return ERROR_NOT_AUTHORIZED;
    }
    
    // Отправляем всем через PUB сокет
    char formatted_msg[1024];
    snprintf(formatted_msg, sizeof(formatted_msg),
            "BROADCAST:%s:*:%s:%lu",
            msg->sender, msg->text, (unsigned long)msg->timestamp);
    
    int rc = zmq_send(server->pub_socket, formatted_msg, strlen(formatted_msg), 0);
    if (rc == -1) {
        SERVER_LOG_ERROR(server, "Failed to broadcast message: %s", 
                        zmq_strerror(errno));
        return ERROR_INTERNAL_SERVER;
    }
    
    SERVER_LOG_INFO(server, "Broadcast from %s: %s", msg->sender, msg->text);
    return ERROR_SUCCESS;
}

// 4. Исправляем имя поля в структуре
int server_send_response(Server *server, void *socket, 
                        MessageType original_type, 
                        uint32_t original_id,
                        ErrorCode err_code,
                        const char *info) {
    if (!server || !socket) return -1;
    
    ServerResponse resp;
    memset(&resp, 0, sizeof(ServerResponse));
    
    resp.original_type = original_type;
    resp.original_id = original_id;
    resp.err_code = err_code;
    
    if (info) {
        safe_strcpy(resp.info, info, MAX_MESSAGE_LENGTH);
    } else {
        // Автоматическое сообщение по коду ошибки
        switch (err_code) {
            case ERROR_SUCCESS:
                safe_strcpy(resp.info, "Success", MAX_MESSAGE_LENGTH);
                break;
            case ERROR_LOGIN_EXISTS:
                safe_strcpy(resp.info, "Login already exists", MAX_MESSAGE_LENGTH);
                break;
            case ERROR_LOGIN_NOT_FOUND:
                safe_strcpy(resp.info, "User not found", MAX_MESSAGE_LENGTH);
                break;
            case ERROR_USER_OFFLINE:
                safe_strcpy(resp.info, "User is offline", MAX_MESSAGE_LENGTH);
                break;
            default:
                safe_strcpy(resp.info, "Unknown error", MAX_MESSAGE_LENGTH);
                break;
        }
    }
    
    int rc = zmq_send(socket, &resp, sizeof(ServerResponse), 0);
    if (rc == -1) {
        SERVER_LOG_ERROR(server, "Failed to send response: %s", zmq_strerror(errno));
        return -1;
    }
    
    return 0;
}

// 5. Троеточие (...) - это вариадическая функция (variadic function)
// Позволяет принимать переменное количество аргументов
// Используется вместе с va_start, va_arg, va_end
void server_log(Server *server, const char *level, const char *format, ...) {
    if (!server || !server->log_file) return;
    
    time_t now = time(NULL);
    if (now == (time_t)-1) return;
    
    struct tm *tm_info = localtime(&now);
    if (!tm_info) return;
    
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    fprintf(server->log_file, "[%s] [%s] ", time_str, level);
    
    va_list args;
    va_start(args, format);
    vfprintf(server->log_file, format, args);
    va_end(args);
    
    fprintf(server->log_file, "\n");
    fflush(server->log_file);
}

// ============================================
// Обработка регистрации
// ============================================
int server_handle_register(Server *server, Message *msg, void *client_socket) {
    if (!server || !msg) {
        return ERROR_INVALID_MESSAGE;
    }
    
    SERVER_LOG_INFO(server, "Registration attempt from: %s", msg->sender);
    
    if (!is_valid_login(msg->sender)) {
        SERVER_LOG_ERROR(server, "Invalid login format: %s", msg->sender);
        return ERROR_INVALID_LOGIN;
    }
    
    int result = user_manager_register(server->user_manager, msg->sender, 
                                      client_socket, 0);
    if (result != ERROR_SUCCESS) {
        SERVER_LOG_ERROR(server, "Registration failed for %s: %d", 
                        msg->sender, result);
        return result;
    }

    char welcome_msg[256];
    time_t now = time(NULL);
    snprintf(welcome_msg, sizeof(welcome_msg), 
             "SYS:system:*:Welcome %s to the chat!:%lu",
             msg->sender, (unsigned long)now);
    
    printf("[SERVER DEBUG] Publishing welcome: %s\n", welcome_msg);
    int sent = zmq_send(server->pub_socket, welcome_msg, strlen(welcome_msg), 0);
    printf("[SERVER DEBUG] zmq_send returned: %d\n", sent);
    
    // Отправляем накопленные offline-сообщения
    server_deliver_offline_messages(server, msg->sender);
    
    SERVER_LOG_INFO(server, "User registered: %s", msg->sender);
    return ERROR_SUCCESS;
}

// ============================================
// Обработка входа
// ============================================
int server_handle_login(Server *server, Message *msg, void *client_socket) {
    if (!server || !msg || !client_socket) {
        return ERROR_INVALID_MESSAGE;
    }
    
    SERVER_LOG_INFO(server, "Login attempt from: %s", msg->sender);
    
    if (!is_valid_login(msg->sender)) {
        SERVER_LOG_ERROR(server, "Invalid login format: %s", msg->sender);
        return ERROR_INVALID_LOGIN;
    }
    
    int result = user_manager_login(server->user_manager, msg->sender, 
                                   client_socket, 0);
    if (result != ERROR_SUCCESS) {
        SERVER_LOG_ERROR(server, "Login failed for %s: %d", 
                        msg->sender, result);
        return result;
    }
    
    // Отправляем накопленные offline-сообщения
    server_deliver_offline_messages(server, msg->sender);
    
    SERVER_LOG_INFO(server, "User logged in: %s", msg->sender);
    return ERROR_SUCCESS;
}

// ============================================
// Обработка выхода
// ============================================
int server_handle_logout(Server *server, Message *msg) {
    if (!server || !msg) {
        return ERROR_INVALID_MESSAGE;
    }
    
    SERVER_LOG_INFO(server, "Logout request from: %s", msg->sender);
    
    int result = user_manager_logout(server->user_manager, msg->sender);
    if (result != ERROR_SUCCESS) {
        SERVER_LOG_ERROR(server, "Logout failed for %s: %d", 
                        msg->sender, result);
        return result;
    }
    
    SERVER_LOG_INFO(server, "User logged out: %s", msg->sender);
    return ERROR_SUCCESS;
}

// ============================================
// Обработка текстового сообщения
// ============================================
int server_handle_text_message(Server *server, Message *msg) {
    if (!server || !msg) {
        return ERROR_INVALID_MESSAGE;
    }
    
    SERVER_LOG_INFO(server, "Message from %s to %s: %s", 
                    msg->sender, msg->receiver, msg->text);
    
    // Проверяем отправителя
    if (!user_manager_exists(server->user_manager, msg->sender)) {
        SERVER_LOG_ERROR(server, "Sender not registered: %s", msg->sender);
        return ERROR_NOT_AUTHORIZED;
    }
    
    // Проверяем получателя
    if (!user_manager_exists(server->user_manager, msg->receiver)) {
        SERVER_LOG_ERROR(server, "Receiver not found: %s", msg->receiver);
        return ERROR_LOGIN_NOT_FOUND;
    }
    
    // Проверяем, онлайн ли получатель
    if (user_manager_is_online(server->user_manager, msg->receiver)) {
        // Отправляем немедленно через PUB сокет
        char formatted_msg[1024];
        snprintf(formatted_msg, sizeof(formatted_msg), 
                "MSG:%s:%s:%s:%lu",
                msg->sender, msg->receiver, msg->text, 
                (unsigned long)msg->timestamp);
        
        int rc = zmq_send(server->pub_socket, formatted_msg, 
                         strlen(formatted_msg), 0);
        if (rc == -1) {
            SERVER_LOG_ERROR(server, "Failed to send message via PUB: %s", 
                            zmq_strerror(errno));
            return ERROR_INTERNAL_SERVER;
        }
        
        SERVER_LOG_INFO(server, "Message delivered to online user: %s", 
                       msg->receiver);
    } else {
        // Сохраняем как offline-сообщение
        int result = message_store_add(server->message_store, msg);
        if (result != ERROR_SUCCESS) {
            SERVER_LOG_ERROR(server, "Failed to store offline message: %d", result);
            return result;
        }
        
        SERVER_LOG_INFO(server, "Message stored for offline user: %s", 
                       msg->receiver);
    }
    
    return ERROR_SUCCESS;
}

// ============================================
// Обработка запроса списка пользователей
// ============================================
int server_handle_get_users(Server *server, Message *msg, void *client_socket) {
    if (!server || !msg || !client_socket) {
        return ERROR_INVALID_MESSAGE;
    }
    
    SERVER_LOG_INFO(server, "User list request from: %s", msg->sender);
    
    if (!user_manager_exists(server->user_manager, msg->sender)) {
        SERVER_LOG_ERROR(server, "Unauthorized request from: %s", msg->sender);
        return ERROR_NOT_AUTHORIZED;
    }
    
    int count = 0;
    char **logins = user_manager_get_online_logins(server->user_manager, &count);
    if (!logins) {
        SERVER_LOG_ERROR(server, "Failed to get user list");
        return ERROR_INTERNAL_SERVER;
    }
    
    // Формируем ответ
    char user_list[2048] = "Online users: ";
    int offset = strlen(user_list);
    
    for (int i = 0; i < count; i++) {
        int remaining = sizeof(user_list) - offset;
        if (remaining <= 0) break;
        
        int written = snprintf(user_list + offset, remaining, 
                              "%s%s", (i > 0 ? ", " : ""), logins[i]);
        if (written > 0) {
            offset += written;
        }
        
        free(logins[i]);
    }
    free(logins);
    
    // ОТПРАВЛЯЕМ ОСОБЫЙ ОТВЕТ (не стандартный)
    // Для GET_USERS нужен специальный формат с данными
    ServerResponse resp;
    memset(&resp, 0, sizeof(ServerResponse));
    resp.original_type = msg->type;
    resp.original_id = msg->message_id;
    resp.err_code = ERROR_SUCCESS;
    safe_strcpy(resp.info, user_list, MAX_MESSAGE_LENGTH);
    
    int rc = zmq_send(client_socket, &resp, sizeof(ServerResponse), 0);
    if (rc == -1) {
        SERVER_LOG_ERROR(server, "Failed to send user list: %s", 
                        zmq_strerror(errno));
        return ERROR_INTERNAL_SERVER;
    }
    
    SERVER_LOG_INFO(server, "User list sent to: %s (%d users)", 
                   msg->sender, count);
    return ERROR_SUCCESS;  // Возвращаем успех, но ответ уже отправлен
}

// ============================================
// Обработка смены статуса
// ============================================
int server_handle_set_status(Server *server, Message *msg) {
    if (!server || !msg) {
        return ERROR_INVALID_MESSAGE;
    }
    
    SERVER_LOG_INFO(server, "Status update from %s: %s", 
                   msg->sender, msg->text);
    
    // Парсим новый статус из текста сообщения
    UserStatus new_status = string_to_status(msg->text);
    if (new_status == USER_STATUS_OFFLINE && 
        strcmp(msg->text, "offline") != 0) {  // Используем strcmp вместо strcasecmp
        SERVER_LOG_ERROR(server, "Invalid status: %s", msg->text);
        return ERROR_INVALID_MESSAGE;
    }
    
    int result = user_manager_update_status(server->user_manager, 
                                           msg->sender, new_status);
    if (result != ERROR_SUCCESS) {
        SERVER_LOG_ERROR(server, "Failed to update status for %s: %d", 
                        msg->sender, result);
        return result;
    }
    
    // Уведомляем всех о смене статуса
    char status_msg[256];
    const char *status_str = status_to_string(new_status);
    snprintf(status_msg, sizeof(status_msg),
            "User %s is now %s", msg->sender, status_str);
    
    // Формируем системное сообщение
    Message broadcast;
    memset(&broadcast, 0, sizeof(Message));
    broadcast.type = MSG_TYPE_SYSTEM_MESSAGE;
    broadcast.message_id = generate_message_id();
    broadcast.timestamp = time(NULL);
    safe_strcpy(broadcast.sender, "system", MAX_LOGIN_LENGTH);
    safe_strcpy(broadcast.receiver, "*", MAX_LOGIN_LENGTH);
    safe_strcpy(broadcast.text, status_msg, MAX_MESSAGE_LENGTH);
    broadcast.flags = FLAG_SYSTEM;
    
    // Отправляем через PUB сокет
    // Безопасный форматирование с проверкой
    char formatted[2048];  // Увеличиваем буфер в 4 раза
    
    // Используем strcpy и strcat для избежания warning
    strcpy(formatted, "SYS:");
    strcat(formatted, broadcast.sender);
    strcat(formatted, ":");
    strcat(formatted, broadcast.receiver);
    strcat(formatted, ":");
    strcat(formatted, broadcast.text);
    
    char timestamp_str[64];
    snprintf(timestamp_str, sizeof(timestamp_str), ":%lu", 
             (unsigned long)broadcast.timestamp);
    strcat(formatted, timestamp_str);
    
    int rc = zmq_send(server->pub_socket, formatted, strlen(formatted), 0);
    if (rc == -1) {
        SERVER_LOG_ERROR(server, "Failed to send status update: %s", 
                        zmq_strerror(errno));
    }
    
    SERVER_LOG_INFO(server, "Status updated for %s: %s", 
                   msg->sender, status_str);
    return ERROR_SUCCESS;
}

// ============================================
// Доставка offline-сообщений
// ============================================
int server_deliver_offline_messages(Server *server, const char *login) {
    if (!server || !login) {
        return ERROR_INVALID_MESSAGE;
    }
    
    Message *offline_messages = NULL;
    int message_count = 0;
    
    int result = message_store_deliver_for_user(server->message_store, login,
                                               &offline_messages, &message_count);
    if (result != ERROR_SUCCESS) {
        SERVER_LOG_ERROR(server, "Failed to get offline messages for %s: %d", 
                        login, result);
        return result;
    }
    
    if (message_count == 0) {
        SERVER_LOG_INFO(server, "No offline messages for %s", login);
        return ERROR_SUCCESS;
    }
    
    // Отправляем каждое offline-сообщение
    for (int i = 0; i < message_count; i++) {
        char formatted_msg[1024];
        snprintf(formatted_msg, sizeof(formatted_msg),
                "OFFLINE:%s:%s:%s:%lu",
                offline_messages[i].sender, 
                offline_messages[i].receiver,
                offline_messages[i].text,
                (unsigned long)offline_messages[i].timestamp);
        
        zmq_send(server->pub_socket, formatted_msg, strlen(formatted_msg), 0);
        
        SERVER_LOG_INFO(server, "Delivered offline message from %s to %s",
                       offline_messages[i].sender, login);
    }
    
    free(offline_messages);
    SERVER_LOG_INFO(server, "Delivered %d offline messages to %s", 
                   message_count, login);
    return ERROR_SUCCESS;
}