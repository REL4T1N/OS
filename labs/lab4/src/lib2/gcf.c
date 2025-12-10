#include <stdlib.h>  // для abs()

int GCF(int A, int B) {
    // Взятие модулей
    int a = abs(A);
    int b = abs(B);
    
    // Обработка краевых случаев
    if (a == 0) return b;
    if (b == 0) return a;
    
    // Наивный алгоритм: перебор от меньшего числа
    int divisor = (a < b) ? a : b;
    
    while (divisor > 1) {
        if (a % divisor == 0 && b % divisor == 0) {
            return divisor;
        }
        divisor--;
    }
    
    // Если не нашли делителей больше 1, возвращаем 1
    return 1;
}