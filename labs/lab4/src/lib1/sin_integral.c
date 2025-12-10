#include <math.h>
#include <stdio.h>

float sinIntegral(float A, float B, float e) {
    // Проверка корректности входных данных
    if (B <= A || e <= 0.0f) {
        fprintf(stderr, "Warning: Invalid input for sinIntegral: A=%.2f, B=%.2f, e=%.2f\n", A, B, e);
        return 0.0f;
    }
    
    float result = 0.0f;
    float current = A;
    
    // Метод левых прямоугольников
    while (current < B) {
        float width = (current + e < B) ? e : (B - current);
        float height = sinf(current);  // Значение функции на левой границе
        result += height * width;
        current += width;
    }
    
    return result;
}