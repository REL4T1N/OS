# Компилятор и флаги
CC = gcc
CFLAGS = -Wall -Wextra -fPIC -I./include
LDFLAGS = -shared -lm
RM = rm -f

# Директории
SRC_DIR = src
BUILD_DIR = build
LIB_DIR = $(BUILD_DIR)/lib

# Библиотеки
LIB1 = $(LIB_DIR)/lib1.so
LIB2 = $(LIB_DIR)/lib2.so

# Программы
PROGRAM1 = $(BUILD_DIR)/program1
PROGRAM2 = $(BUILD_DIR)/program2

# Исходные файлы для библиотек
LIB1_SRC = $(SRC_DIR)/lib1/sin_integral.c $(SRC_DIR)/lib1/gcf.c
LIB2_SRC = $(SRC_DIR)/lib2/sin_integral.c $(SRC_DIR)/lib2/gcf.c

# Основная цель - собрать всё
all: directories libraries programs

# Создание необходимых директорий
directories:
	@mkdir -p $(BUILD_DIR) $(LIB_DIR)

# Цель для сборки только библиотек
libraries: $(LIB1) $(LIB2)

# Сборка библиотеки Variant1
$(LIB1): $(LIB1_SRC)
	$(CC) $(CFLAGS) $(LIB1_SRC) $(LDFLAGS) -o $@
	@echo "Built: $(LIB1)"

# Сборка библиотеки Variant2
$(LIB2): $(LIB2_SRC)
	$(CC) $(CFLAGS) $(LIB2_SRC) $(LDFLAGS) -o $@
	@echo "Built: $(LIB2)"

# Цель для сборки только программ
programs: $(PROGRAM1) $(PROGRAM2)

# Сборка program1 (вызывает вложенный Makefile)
$(PROGRAM1): libraries
	@echo "Building program1..."
	@cd src/prog1 && $(MAKE)

# Сборка program2
$(PROGRAM2): libraries
	@echo "Building program2..."
	@cd src/prog2 && $(MAKE)

# Очистка только библиотек
clean:
	$(RM) -r $(BUILD_DIR)
	@echo "Cleaned build directory"

# Полная очистка всего проекта
clean-all: clean
	@echo "Cleaning program1..."
	@cd src/prog1 && $(MAKE) clean 2>/dev/null || true
	@echo "Cleaning program2..."
	@cd src/prog2 && $(MAKE) clean 2>/dev/null || true

# Запуск программы 1
run1: all
	@echo "Running program1..."
	@LD_LIBRARY_PATH=$(LIB_DIR):$$LD_LIBRARY_PATH $(PROGRAM1)

# Запуск программы 2
run2: all
	@echo "Running program2..."
	@LD_LIBRARY_PATH=$(LIB_DIR):$$LD_LIBRARY_PATH $(PROGRAM2)

# Быстрый тест
test: all
	@echo "=== Quick test of program1 ==="
	@LD_LIBRARY_PATH=$(LIB_DIR):$$LD_LIBRARY_PATH echo -e "1 0 3.14159 0.01\n2 48 18\n3" | $(PROGRAM1) | grep -E "Result|Warning|Error" || true
	@echo ""
	@echo "=== Quick test of program2 ==="
	@LD_LIBRARY_PATH=$(LIB_DIR):$$LD_LIBRARY_PATH echo -e "1 0 3.14159 0.01\n2 48 18\n0\n1 0 3.14159 0.01\n2 48 18\n3" | $(PROGRAM2) | grep -E "Result|Switched|Warning|Error" || true
	@echo "=== Test completed ==="

# Strace для program1
strace1: all
	@echo "Running strace on program1..."
	@strace -f -o strace_program1.txt ./build/program1

# Strace для program2
strace2: all
	@echo "Running strace on program2..."
	@strace -f -o strace_program2.txt ./build/program2

# Strace с тестовыми данными
strace-test1: all
	@echo "Running strace on program1 with test data..."
	@echo -e "1 0 3.14159 0.1\n2 48 18\n3" | strace -f -o strace_test1.txt ./build/program1

strace-test2: all
	@echo "Running strace on program2 with test data..."
	@echo -e "1 0 3.14159 0.1\n2 48 18\n0\n1 0 3.14159 0.1\n2 48 18\n3" | strace -f -o strace_test2.txt ./build/program2

# Анализ strace результатов
analyze-strace:
	@echo "=== Analysis of strace_program1.txt ==="
	@grep -E "(open|mmap|dlopen|dlsym|close|exec)" strace_program1.txt | head -30
	@echo ""
	@echo "=== Analysis of strace_program2.txt ==="
	@grep -E "(open|mmap|dlopen|dlsym|close|exec)" strace_program2.txt | head -30

# Очистка strace файлов
clean-strace:
	$(RM) strace_*.txt

# Информация о проекте
info:
	@echo "=== Project Structure ==="
	@echo "Libraries:"
	@echo "  $(LIB1)"
	@echo "  $(LIB2)"
	@echo ""
	@echo "Programs:"
	@echo "  $(PROGRAM1)"
	@echo "  $(PROGRAM2)"
	@echo ""
	@echo "Available make targets:"
	@echo "  make all              - Build everything (default)"
	@echo "  make libraries        - Build only libraries"
	@echo "  make programs         - Build only programs (requires libraries)"
	@echo "  make clean            - Clean only libraries"
	@echo "  make clean-all        - Clean everything"
	@echo "  make run1             - Run program1"
	@echo "  make run2             - Run program2"
	@echo "  make test             - Run quick test"
	@echo "  make strace1          - Run program1 with strace"
	@echo "  make strace2          - Run program2 with strace"
	@echo "  make strace-test1     - Run program1 with strace and test data"
	@echo "  make strace-test2     - Run program2 with strace and test data"
	@echo "  make analyze-strace   - Analyze strace output"
	@echo "  make clean-strace     - Clean strace files"
	@echo "  make info             - Show this information"

.PHONY: all directories libraries programs clean clean-all run1 run2 test info \
        strace1 strace2 strace-test1 strace-test2 analyze-strace clean-strace