#include <stdlib.h>  // для abs()

int GCF(int A, int B) {
    // Взятие модулей для корректной работы с отрицательными числами
    int a = abs(A);
    int b = abs(B);
    
    // Обработка краевых случаев
    if (a == 0) return b;
    if (b == 0) return a;
    
    // Алгоритм Евклида (итеративная версия)
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    
    return a;
}