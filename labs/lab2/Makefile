CC = gcc
CFLAGS = -Wall -Wextra -pthread -O2 -Isrc
SRCDIR = src
SOURCES = $(SRCDIR)/main.c $(SRCDIR)/merge_sort.c $(SRCDIR)/benchmark.c
TARGET = parallel_sort

# Директории для результатов
RESULTS_DIR = results
TEST_DIR = $(RESULTS_DIR)/test
BENCHMARK_DIR = $(RESULTS_DIR)/benchmark
TEST_LOGS_DIR = $(TEST_DIR)/logs
TEST_GRAPHICS_DIR = $(TEST_DIR)/graphics
BENCHMARK_LOGS_DIR = $(BENCHMARK_DIR)/logs
BENCHMARK_GRAPHICS_DIR = $(BENCHMARK_DIR)/graphics

# Файлы логов
TEST_LOGFILE = $(TEST_LOGS_DIR)/test_logs.txt
BENCHMARK_LOGFILE = $(BENCHMARK_LOGS_DIR)/benchmark.txt

VENV = venv
PYTHON = $(VENV)/bin/python3

.PHONY: all clean run test benchmark graphics test-size test-threads test-threshold test-single help venv directories

# Создание директорий
directories:
	@mkdir -p $(TEST_LOGS_DIR) $(TEST_GRAPHICS_DIR) $(BENCHMARK_LOGS_DIR) $(BENCHMARK_GRAPHICS_DIR)

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

# TEST: запускает каждый тест по 1 разу, вывод в консоль и файл
test: directories $(TARGET)
	@echo "=== ЗАПУСК ТЕСТОВ (1 итерация) ===" > $(TEST_LOGFILE)
	@echo "Время начала: $(shell date)" >> $(TEST_LOGFILE)
	@echo "" >> $(TEST_LOGFILE)
	./$(TARGET) -all | tee -a $(TEST_LOGFILE)
	@echo "Тесты завершены. Логи сохранены в: $(TEST_LOGFILE)"

# BENCHMARK: запускает каждый тест по 5 раз, вывод только в файл
benchmark: directories $(TARGET)
	@echo "=== ЗАПУСК БЕНЧМАРКОВ (5 итераций) ===" > $(BENCHMARK_LOGFILE)
	@echo "Время начала: $(shell date)" >> $(BENCHMARK_LOGFILE)
	@echo "" >> $(BENCHMARK_LOGFILE)
	@for i in 1 2 3 4 5; do \
		echo "=== Итерация $$i ===" >> $(BENCHMARK_LOGFILE); \
		./$(TARGET) -all >> $(BENCHMARK_LOGFILE); \
		echo "" >> $(BENCHMARK_LOGFILE); \
	done
	@echo "Бенчмарк завершен. Результаты в: $(BENCHMARK_LOGFILE)"

# Отдельные тесты (по 1 разу, только в консоль)
test-size: $(TARGET)
	./$(TARGET) -size

# ИЗМЕНИЛ: test-depth -> test-threads
test-threads: $(TARGET)
	./$(TARGET) -threads

test-threshold: $(TARGET)
	./$(TARGET) -threshold

# ИЗМЕНИЛ: убрал -d параметр
test-single: $(TARGET)
	./$(TARGET) -s 10000000 -t 8 -p 1000

run: $(TARGET)
	./$(TARGET)

# Создание виртуального окружения и установка зависимостей
venv:
	@if [ ! -d "$(VENV)" ]; then \
		echo "Создание виртуального окружения..."; \
		python3 -m venv $(VENV); \
		$(PYTHON) -m pip install --upgrade pip; \
		$(PYTHON) -m pip install matplotlib numpy; \
		echo "Виртуальное окружение создано и зависимости установлены."; \
	else \
		echo "Виртуальное окружение уже существует."; \
	fi

# Построение графиков из test_logs.txt (и показ и сохранение)
graphics: venv directories
	$(PYTHON) scripts/get_graphics.py $(TEST_LOGFILE) $(TEST_GRAPHICS_DIR)/test_
	@echo "Графики тестов сохранены в: $(TEST_GRAPHICS_DIR)/"

# Построение графиков из benchmark.txt (и показ и сохранение)
benchmark-graphics: venv directories
	$(PYTHON) scripts/get_graphics.py $(BENCHMARK_LOGFILE) $(BENCHMARK_GRAPHICS_DIR)/benchmark_
	@echo "Графики бенчмарков сохранены в: $(BENCHMARK_GRAPHICS_DIR)/"

# Полный пайплайн: тесты + графики
full-test: test graphics
	@echo "Полный тест завершен!"

full-benchmark: benchmark benchmark-graphics
	@echo "Полный бенчмарк завершен!"

clean:
	rm -f $(TARGET)
	rm -rf $(RESULTS_DIR)
	rm -rf $(VENV)

clean-venv:
	rm -rf $(VENV)

clean-results:
	rm -rf $(RESULTS_DIR)

help:
	@echo "Доступные команды:"
	@echo "  all               - компиляция программы"
	@echo "  run               - запуск с параметрами по умолчанию"
	@echo "  test              - все тесты по 1 разу (консоль + $(TEST_LOGFILE))"
	@echo "  benchmark         - все тесты по 5 раз (только $(BENCHMARK_LOGFILE))"
	@echo "  graphics          - графики из тестов (показ + сохранение в $(TEST_GRAPHICS_DIR))"
	@echo "  benchmark-graphics - графики из бенчмарков (показ + сохранение в $(BENCHMARK_GRAPHICS_DIR))"
	@echo "  full-test         - полный тест (тесты + графики)"
	@echo "  full-benchmark    - полный бенчмарк (бенчмарки + графики)"
	@echo "  test-size         - тест размеров (1 раз, только консоль)"
	@echo "  test-threads      - тест количества потоков (1 раз, только консоль)"
	@echo "  test-threshold    - тест порогов (1 раз, только консоль)"
	@echo "  test-single       - одиночный тест (1 раз, только консоль)"
	@echo "  venv              - создание виртуального окружения"
	@echo "  clean             - удаление ВСЕХ сгенерированных файлов"
	@echo "  clean-venv        - удаление только виртуального окружения"
	@echo "  clean-results     - удаление только результатов"
	@echo "  help              - показать эту справку"