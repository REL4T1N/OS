# Makefile для мессенджера на ZeroMQ

# Компилятор и флаги
CC = gcc
CFLAGS = -Wall -Wextra -g -std=c11 -I./src -I./src/common
LDFLAGS = -lzmq -lpthread

# Директории
SRC_DIR = src
BUILD_DIR = build
COMMON_DIR = $(SRC_DIR)/common
SERVER_DIR = $(SRC_DIR)/server
CLIENT_DIR = $(SRC_DIR)/client
TEST_DIR = test

# Цели
all: server

# Общие модули
COMMON_OBJS = $(BUILD_DIR)/utils.o

$(BUILD_DIR)/utils.o: $(COMMON_DIR)/utils.c $(COMMON_DIR)/utils.h \
                      $(COMMON_DIR)/protocol.h $(COMMON_DIR)/message_types.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

common: $(COMMON_OBJS)

# Сервер
SERVER_OBJS = $(COMMON_OBJS) \
              $(BUILD_DIR)/server_main.o \
              $(BUILD_DIR)/server_core.o \
              $(BUILD_DIR)/user_manager.o \
              $(BUILD_DIR)/message_store.o

# Клиент
CLIENT_OBJS = $(BUILD_DIR)/client_main.o \
              $(BUILD_DIR)/client_core.o \
              $(BUILD_DIR)/network_handler.o \
              $(BUILD_DIR)/ui_handler.o

$(BUILD_DIR)/server_main.o: $(SERVER_DIR)/main.c $(SERVER_DIR)/server_core.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/server_core.o: $(SERVER_DIR)/server_core.c $(SERVER_DIR)/server_core.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/user_manager.o: $(SERVER_DIR)/user_manager.c $(SERVER_DIR)/user_manager.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/message_store.o: $(SERVER_DIR)/message_store.c $(SERVER_DIR)/message_store.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/client_main.o: $(CLIENT_DIR)/main.c $(CLIENT_DIR)/client_core.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/client_core.o: $(CLIENT_DIR)/client_core.c $(CLIENT_DIR)/client_core.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/network_handler.o: $(CLIENT_DIR)/network_handler.c $(CLIENT_DIR)/network_handler.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ui_handler.o: $(CLIENT_DIR)/ui_handler.c $(CLIENT_DIR)/ui_handler.h
	$(CC) $(CFLAGS) -c $< -o $@

server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/server $(SERVER_OBJS) $(LDFLAGS)
	@echo "Server built: ./build/server"

# Клиент
client: $(COMMON_OBJS) $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/client $(COMMON_OBJS) $(CLIENT_OBJS) $(LDFLAGS)
	@echo "Client built: ./build/client"

# Тесты
TEST_OBJS = $(COMMON_OBJS) $(BUILD_DIR)/test_server.o

$(BUILD_DIR)/test_server.o: $(TEST_DIR)/test_server.c $(SERVER_DIR)/server_core.h
	$(CC) $(CFLAGS) -c $< -o $@

test_server: $(TEST_OBJS) $(BUILD_DIR)/server_core.o
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_server $(TEST_OBJS) $(BUILD_DIR)/server_core.o $(LDFLAGS)
	@echo "Test server built: ./build/test_server"

# Тестовый файл сервера
$(TEST_DIR)/test_server.c:
	@mkdir -p $(TEST_DIR)
	@echo '// Простой тест сервера' > $@
	@echo '#include <stdio.h>' >> $@
	@echo '#include "src/server/server_core.h"' >> $@
	@echo 'int main() {' >> $@
	@echo '    printf("Testing server creation...\\n");' >> $@
	@echo '    Server* s = server_create("*", 5555);' >> $@
	@echo '    if (!s) { printf("FAIL\\n"); return 1; }' >> $@
	@echo '    printf("OK\\n");' >> $@
	@echo '    server_destroy(s);' >> $@
	@echo '    return 0;' >> $@
	@echo '}' >> $@

# Быстрый тест
quick_test: server test_server
	@echo "=== Quick test ==="
	@echo "1. Creating server..."
	@./build/test_server && echo "✓ Server creation test passed" || echo "✗ Server creation test failed"
	@echo "2. Checking server binary..."
	@file ./build/server && echo "✓ Server binary exists" || echo "✗ No server binary"

# Запуск сервера
run: server
	@echo "Starting server..."
	@./build/server

# Запуск с параметрами
run_with_args: server
	@./build/server $(ARGS)

# Очистка
clean:
	rm -rf $(BUILD_DIR)
	rm -f *.log server.log

# Полная очистка
distclean: clean
	rm -f $(TEST_DIR)/test_server.c

# Структура проекта
init:
	mkdir -p $(SRC_DIR)/{common,server,client} $(BUILD_DIR) data test

.PHONY: all common server client test_server quick_test run run_with_args clean distclean init