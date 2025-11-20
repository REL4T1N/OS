# Компилятор и флаги
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
LDFLAGS = -lrt
TARGET_PARENT = parent
TARGET_CHILD = child

# Основные цели
all: $(TARGET_PARENT) $(TARGET_CHILD)

$(TARGET_PARENT): parent.c shared_memory.c shared_memory.h
	$(CC) $(CFLAGS) -o $(TARGET_PARENT) parent.c shared_memory.c $(LDFLAGS)

$(TARGET_CHILD): child.c shared_memory.c shared_memory.h
	$(CC) $(CFLAGS) -o $(TARGET_CHILD) child.c shared_memory.c $(LDFLAGS)

# Отладочные цели
debug: CFLAGS += -DDEBUG -O0
debug: all

# Strace цели
strace-parent: $(TARGET_PARENT)
	strace -f -o strace-parent.log ./$(TARGET_PARENT)

strace-child: $(TARGET_CHILD)
	strace -o strace-child.log ./$(TARGET_CHILD)

strace-both: $(TARGET_PARENT) $(TARGET_CHILD)
	strace -f -o strace-both.log ./$(TARGET_PARENT)

# Валидация shared memory
check-shm:
	@echo "=== Проверка shared memory объектов ==="
	@ls -la /dev/shm/ | grep -E "(sum_calc|shm)" || echo "Shared memory объекты не найдены"
	@echo ""

# Очистка
clean:
	rm -f $(TARGET_PARENT) $(TARGET_CHILD) *.o

clean-all: clean
	rm -f *.log
	-sudo rm -f /dev/shm/sum_calc_shm_1
	-sudo rm -f /dev/shm/sum_calc_shm_2
	-sudo rm -f /dev/shm/*sum_calc*

# Просмотр логов
view-strace:
	@if [ -f strace-parent.log ]; then \
		echo "=== Strace parent ==="; \
		tail -20 strace-parent.log; \
	fi
	@if [ -f strace-child.log ]; then \
		echo "=== Strace child ==="; \
		tail -20 strace-child.log; \
	fi

# Тестирование
test: all
	@echo "=== Создание тестового файла ==="
	@echo "1 2 3" > test_input.txt
	@echo "4 5" >> test_input.txt
	@echo "10 20 30 40" >> test_input.txt
	@echo "=== Запуск программы ==="
	./$(TARGET_PARENT)

# Помощь
help:
	@echo "Доступные цели:"
	@echo "  all          - компиляция родительской и дочерней программ"
	@echo "  debug        - компиляция с отладочной информацией"
	@echo "  strace-parent- запуск родителя под strace"
	@echo "  strace-both  - запуск всей программы под strace"
	@echo "  check-shm    - проверка shared memory объектов"
	@echo "  clean        - удаление скомпилированных программ"
	@echo "  clean-all    - полная очистка (включая shared memory)"
	@echo "  test         - автоматическое тестирование"
	@echo "  view-strace  - просмотр логов strace"
	@echo "  help         - эта справка"

.PHONY: all debug strace-parent strace-child strace-both check-shm clean clean-all test view-strace help